#pragma once
#include "PerMaterialIndirectUniforms.h"
#include "../vulkan/SwapChain.h"
#include "../vulkan/GlobalDeviceObjects.h"
#include "../vulkan/Buffer.h"
#include "../vulkan/DescriptorSet.h"
#include "../vulkan/UniformBuffer.h"
#include "../vulkan/ShaderStorageBuffer.h"
#include "UniformData.h"
#include "Material.h"

bool PerMaterialIndirectUniforms::Init(const std::shared_ptr<PerMaterialIndirectUniforms>& pSelf)
{
	if (!UniformDataStorage::Init(pSelf, sizeof(m_perMaterialIndirectIndex), true))
		return false;
	return true;
}

std::shared_ptr<PerMaterialIndirectUniforms> PerMaterialIndirectUniforms::Create()
{
	std::shared_ptr<PerMaterialIndirectUniforms> pPerMaterialIndirectUniforms = std::make_shared<PerMaterialIndirectUniforms>();
	if (pPerMaterialIndirectUniforms.get() && pPerMaterialIndirectUniforms->Init(pPerMaterialIndirectUniforms))
		return pPerMaterialIndirectUniforms;
	return nullptr;
}

void PerMaterialIndirectUniforms::SyncBufferDataInternal()
{
	GetBuffer()->UpdateByteStream(m_perMaterialIndirectIndex, FrameMgr()->FrameIndex() * GetFrameOffset(), sizeof(m_perMaterialIndirectIndex));
}

std::vector<UniformVarList> PerMaterialIndirectUniforms::PrepareUniformVarList() const
{
	return
	{
		{
			DynamicShaderStorageBuffer,
			"PerMaterialIndirectIndex",
			{
				{ OneUnit, "PerObjectIndex" },
				{ OneUnit, "PerMaterialIndex" },
			}
		}
	};
}

uint32_t PerMaterialIndirectUniforms::SetupDescriptorSet(const std::shared_ptr<DescriptorSet>& pDescriptorSet, uint32_t bindingIndex) const
{
	pDescriptorSet->UpdateShaderStorageBufferDynamic(bindingIndex++, std::dynamic_pointer_cast<ShaderStorageBuffer>(GetBuffer()));

	return bindingIndex;
}


