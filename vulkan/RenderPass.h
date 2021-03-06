#pragma once

#include "DeviceObjectBase.h"
#include <mutex>

class CommandBuffer;

class RenderPass : public DeviceObjectBase<RenderPass>
{
public:
	~RenderPass();

public:
	static std::shared_ptr<RenderPass> Create(const std::shared_ptr<Device>& pDevice, const VkRenderPassCreateInfo& renderPassInfo);

protected:
	bool Init(const std::shared_ptr<Device>& pDevice, const std::shared_ptr<RenderPass>& pSelf, const VkRenderPassCreateInfo& renderPassInfo);

	typedef struct _SubpassDef
	{
		std::vector<VkAttachmentReference>		m_colorAttachmentRefs;
		VkAttachmentReference					m_depthStencilAttachmentRef;
		std::vector<VkAttachmentReference>		m_inputAttachmentRefs;
		std::vector<uint32_t>					m_preservAttachmentRefs;
		std::vector<VkAttachmentReference>		m_resolveAttachmentRefs;
	}SubpassDef;

public:
	VkRenderPass GetDeviceHandle() const { return m_renderPass; }
	std::vector<VkAttachmentDescription> GetAttachmentDesc() const { return m_attachmentDescList; }
	std::vector<SubpassDef> GetSubpassDef() const { return m_subpasses; }
	void CacheSecondaryCommandBuffer(const std::shared_ptr<CommandBuffer>& pCmdBuffer);
	void ExecuteCachedSecondaryCommandBuffers(const std::shared_ptr<CommandBuffer>& pCmdBuffer);

protected:
	bool									m_inited = false;
	VkRenderPass							m_renderPass;
	VkRenderPassCreateInfo					m_renderPassInfo;
	std::vector<VkAttachmentDescription>	m_attachmentDescList;
	std::vector<SubpassDef>					m_subpasses;
	std::vector<VkSubpassDescription>		m_subpassDescList;
	std::vector<VkSubpassDependency>		m_subpassDependencyList;

	std::vector<std::shared_ptr<CommandBuffer>> m_secondaryCommandBuffers;
	std::mutex									m_secBufferMutex;
};