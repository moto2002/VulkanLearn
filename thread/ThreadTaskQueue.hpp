#pragma once
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "../vulkan/FrameManager.h"
#include "ThreadWorker.hpp"

class Device;
class CommandBuffer;

class ThreadTaskQueue
{
public:
	ThreadTaskQueue(const std::shared_ptr<Device>& pDevice, uint32_t frameRoundBinCount, const std::shared_ptr<FrameManager>& pFrameMgr)
	{
		m_worker = std::thread(&ThreadTaskQueue::Loop, this);

		if (!m_isWorkerReady)
		{
			m_isWorkerReady = true;
			ThreadWorker::InitThreadWorkers(pDevice, frameRoundBinCount, pFrameMgr);
		}
	}

	~ThreadTaskQueue()
	{
		if (m_worker.joinable())
		{
			WaitForEmptyQueue();
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_isDestroying = true;
			lock.unlock();
			m_condition.notify_one();
			m_worker.join();
		}
	}

public:
	void AddJob(ThreadJobFunc jobFunc, uint32_t frameIndex)
	{
		std::unique_lock<std::mutex> lock(m_queueMutex);
		ThreadWorker::ThreadJob job;
		job.job = jobFunc;
		job.frameIndex = frameIndex;
		job.pThreadTaskQueue = this;
		m_taskQueue.push(job);
		m_condition.notify_one();
	}

	void EndJob()
	{
		std::unique_lock<std::mutex> lock(m_workerCountMutex);
		m_threadWorkerCount--;
		m_workerCountCondition.notify_one();
	}

	void WaitForFree()
	{
		WaitForEmptyQueue();
		WaitForWorkersAllFree();
	}

	void WaitForEmptyQueue()
	{
		std::unique_lock<std::mutex> lock(m_queueMutex);
		m_condition.wait(lock, [this]() { return m_taskQueue.empty() && !m_isSearchingThread; });
	}

	void WaitForWorkersAllFree()
	{
		std::unique_lock<std::mutex> lock(m_workerCountMutex);
		m_workerCountCondition.wait(lock, [this]() { return m_threadWorkerCount == 0; });
	}

	uint32_t GetTaskQueueSize()
	{
		std::unique_lock<std::mutex> lock(m_queueMutex);
		return m_taskQueue.size();
	}

private:
	void Loop()
	{
		while (true)
		{
			ThreadWorker::ThreadJob job;
			job.pThreadTaskQueue = this;
			{
				std::unique_lock<std::mutex> lock(m_queueMutex);
				m_condition.wait(lock, [this]() { return !m_taskQueue.empty() || m_isDestroying; });

				if (m_isDestroying)
				{
					break;
				}

				job = m_taskQueue.front();
				m_taskQueue.pop();

				m_isSearchingThread = true;

				m_condition.notify_one();
			}

			bool shouldExit = false;
			while (!shouldExit)
			{
				std::shared_ptr<ThreadWorker> pTWroker = ThreadWorker::GetCurrentThreadWorker();
				bool isFree = pTWroker->IsTaskQueueFree();
				if (isFree)
				{
					pTWroker->AppendJob(job);
					shouldExit = true;
				}
			}

			{
				std::unique_lock<std::mutex> lock(m_queueMutex);
				m_isSearchingThread = false;
				m_condition.notify_one();
			}
			{
				std::unique_lock<std::mutex> lock(m_workerCountMutex);
				m_threadWorkerCount++;
				m_workerCountCondition.notify_one();
			}
		}
	}

private:
	std::mutex									m_queueMutex;
	std::mutex									m_workerCountMutex;
	std::condition_variable						m_workerCountCondition;
	std::thread									m_worker;
	std::condition_variable						m_condition;
	std::queue<ThreadWorker::ThreadJob>			m_taskQueue;
	uint32_t									m_threadWorkerCount = 0;

	bool m_isSearchingThread =		false;
	bool m_isDestroying =			false;
	static bool m_isWorkerReady;
	uint32_t m_currentWorkerIndex = 0;
};