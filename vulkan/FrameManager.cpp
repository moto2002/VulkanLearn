#include "FrameManager.h"
#include "CommandBuffer.h"
#include "CommandPool.h"
#include "DescriptorPool.h"
#include "DescriptorSet.h"
#include "Fence.h"
#include "GlobalDeviceObjects.h"
#include "SwapChain.h"
#include "PerFrameResource.h"
#include "Fence.h"
#include "Semaphore.h"
#include "CommandBuffer.h"
#include "Queue.h"
#include "../thread/ThreadTaskQueue.hpp"
#include <algorithm>
#include "Semaphore.h"

bool FrameManager::Init(const std::shared_ptr<Device>& pDevice, uint32_t maxFrameCount, const std::shared_ptr<FrameManager>& pSelf)
{
	if (!DeviceObjectBase::Init(pDevice, pSelf))
		return false;

	m_currentFrameIndex = 0;
	m_maxFrameCount = maxFrameCount;

	for (uint32_t i = 0; i < maxFrameCount; i++)
	{
		m_frameResTable[i] = std::vector<std::shared_ptr<PerFrameResource>>();
		m_frameFences.push_back(Fence::Create(pDevice));
		m_threadTaskQueues.push_back(std::make_shared<ThreadTaskQueue>(pDevice, maxFrameCount, pSelf));
		m_acquireDoneSemaphores.push_back(Semaphore::Create(pDevice));
		m_renderDoneSemahpres.push_back(Semaphore::Create(pDevice));
	}

	m_jobStatus.resize(maxFrameCount);

	m_currentFrameIndex = 0;
	m_maxFrameCount = maxFrameCount;
	
	return true;
}

std::shared_ptr<FrameManager> FrameManager::Create(const std::shared_ptr<Device>& pDevice, uint32_t maxFrameCount)
{
	std::shared_ptr<FrameManager> pFrameManager = std::make_shared<FrameManager>();
	if (pFrameManager.get() && pFrameManager->Init(pDevice, maxFrameCount, pFrameManager))
		return pFrameManager;
	return nullptr;
}

std::shared_ptr<PerFrameResource> FrameManager::AllocatePerFrameResource(uint32_t frameIndex)
{
	if (frameIndex < 0 || frameIndex >= m_maxFrameCount)
		return nullptr;

	std::shared_ptr<PerFrameResource> pPerFrameRes = PerFrameResource::Create(GetDevice(), frameIndex);
	m_frameResTable[frameIndex].push_back(pPerFrameRes);
	return pPerFrameRes;
}

void FrameManager::WaitForFence()
{
	WaitForFence(m_currentFrameIndex);
}

void FrameManager::WaitForFence(uint32_t index)
{
	m_frameFences[index]->Wait();
}

void FrameManager::IncFrameIndex()
{
	SetFrameIndex(m_currentFrameIndex + 1);
}

void FrameManager::SetFrameIndex(uint32_t index)
{
	if (m_currentFrameIndex == index)
		return;

	m_currentFrameIndex = index % m_maxFrameCount;

	// We have to flush frame submission just ahead of current frame index, i.e. the oldest frame in the round bin, if there's any
	// Since this frame must be presented in order to make sure current frame can be acquired successfully
	// You can't acquire max frame count, the maximum simuteniously acquired frame count is max frame count -1
	uint32_t oldestFrameIndex = (m_currentFrameIndex + 1) % m_maxFrameCount;
	m_threadTaskQueues[oldestFrameIndex]->WaitForFree();
	FlushCachedSubmission(oldestFrameIndex);
	WaitForFence(oldestFrameIndex);

	// New frame's running resources are no longer in use now, we've already finished waiting for their protective fence
	// They're cleared and okay to remove
	m_submissionInfoTable[oldestFrameIndex].clear();
}

void FrameManager::CacheSubmissioninfo(
	const std::shared_ptr<Queue>& pQueue,
	const std::vector<std::shared_ptr<CommandBuffer>>& cmdBuffer,
	const std::vector<VkPipelineStageFlags>& waitStages,
	bool waitUtilQueueIdle)
{
	uint32_t frameIndex = cmdBuffer[0]->GetCommandPool()->GetPerFrameResource().lock()->GetFrameIndex();
	CacheSubmissioninfo(pQueue, cmdBuffer, { m_acquireDoneSemaphores[frameIndex] }, waitStages, { m_renderDoneSemahpres[frameIndex] }, waitUtilQueueIdle);
}

void FrameManager::CacheSubmissioninfo(
	const std::shared_ptr<Queue>& pQueue,
	const std::vector<std::shared_ptr<CommandBuffer>>& cmdBuffer,
	const std::vector<std::shared_ptr<Semaphore>>& waitSemaphores,
	const std::vector<VkPipelineStageFlags>& waitStages,
	const std::vector<std::shared_ptr<Semaphore>>& signalSemaphores,
	bool waitUtilQueueIdle)
{
	std::unique_lock<std::mutex>(m_mutex);

#ifdef _DEBUG
	{
		ASSERTION(!cmdBuffer[0]->GetCommandPool()->GetPerFrameResource().expired());
		uint32_t frameIndex = cmdBuffer[0]->GetCommandPool()->GetPerFrameResource().lock()->GetFrameIndex();
		for (uint32_t i = 1; i < cmdBuffer.size(); i++)
		{
			ASSERTION(!cmdBuffer[i]->GetCommandPool()->GetPerFrameResource().expired());
			ASSERTION(frameIndex = cmdBuffer[i]->GetCommandPool()->GetPerFrameResource().lock()->GetFrameIndex());
		}
	}
#endif //_DEBUG
	uint32_t frameIndex = cmdBuffer[0]->GetCommandPool()->GetPerFrameResource().lock()->GetFrameIndex();

	SubmissionInfo info = 
	{
		pQueue,
		cmdBuffer,
		waitSemaphores,
		waitStages,
		signalSemaphores,
		waitUtilQueueIdle,
	};

	m_pendingSubmissionInfoTable[frameIndex].push_back(info);
}

void FrameManager::FlushCachedSubmission(uint32_t frameIndex)
{
	if (m_pendingSubmissionInfoTable[frameIndex].size() == 0)
		return;

	if (!m_jobStatus[frameIndex].submissionEnded)
		return;

	// Flush pending cmd buffers
	std::for_each(m_pendingSubmissionInfoTable[frameIndex].begin(), m_pendingSubmissionInfoTable[frameIndex].end(), [this, frameIndex](SubmissionInfo & info)
	{
		m_frameFences[frameIndex]->Reset();
		info.pQueue->SubmitCommandBuffers(info.cmdBuffers, info.waitSemaphores, info.waitStages, info.signalSemaphores, GetFrameFence(frameIndex), info.waitUtilQueueIdle);
	});

	// Add submitted cmd buffer references here, just to make sure they won't be deleted util this submission finished
	m_submissionInfoTable[frameIndex].insert(
		m_submissionInfoTable[frameIndex].end(),
		m_pendingSubmissionInfoTable[frameIndex].begin(),
		m_pendingSubmissionInfoTable[frameIndex].end());

	// Clear pending submissions
	m_pendingSubmissionInfoTable[frameIndex].clear();

	m_jobStatus[frameIndex].callback(frameIndex);
	m_jobStatus[frameIndex].Reset();
}

void FrameManager::AddJobToFrame(ThreadJobFunc jobFunc)
{
	std::unique_lock<std::mutex>(m_mutex);
	m_threadTaskQueues[m_currentFrameIndex]->AddJob(jobFunc, m_currentFrameIndex);
	m_jobStatus[m_currentFrameIndex].numJobs++;
}

void FrameManager::JobDone(uint32_t frameIndex)
{
	std::unique_lock<std::mutex>(m_mutex);
	m_jobStatus[frameIndex].numJobs--;
}

void FrameManager::FlushCurrentFrameJobs()
{
	m_threadTaskQueues[m_currentFrameIndex]->WaitForWorkersAllFree();
	FlushCachedSubmission(m_currentFrameIndex);
}

void FrameManager::EndJobSubmission(std::function<void(uint32_t)> callback)
{
	m_jobStatus[m_currentFrameIndex].callback = callback;
	m_jobStatus[m_currentFrameIndex].submissionEnded = true;
}