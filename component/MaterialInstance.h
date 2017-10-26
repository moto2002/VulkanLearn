#pragma once
#include "../Base/BaseComponent.h"

class DescriptorSet;
class DescriptorPool;
class Material;
class Image;
class CommandBuffer;

class MaterialInstance : public BaseComponent
{
public:
	std::vector<std::shared_ptr<DescriptorSet>> GetDescriptorSets() const { return m_descriptorSets; }
	std::shared_ptr<DescriptorSet> GetDescriptorSet(uint32_t index) const { return m_descriptorSets[index]; }
	std::shared_ptr<Material> GetMaterial() const { return m_pMaterial; }
	uint32_t GetRenderMask() const { return m_renderMask; }
	void SetRenderMask(uint32_t renderMask) { m_renderMask = renderMask; }
	void SetMaterialTexture(uint32_t index, const std::shared_ptr<Image>& pTexture);
	std::shared_ptr<Image> GetMaterialTexture(uint32_t index) { return m_textures[index]; }
	void PrepareMaterial(const std::shared_ptr<CommandBuffer>& pCmdBuffer);

protected:
	bool Init(const std::shared_ptr<MaterialInstance>& pMaterialInstance);
	void BindPipeline(const std::shared_ptr<CommandBuffer>& pCmdBuffer);
	void BindDescriptorSet(const std::shared_ptr<CommandBuffer>& pCmdBuffer);

protected:
	std::vector<std::shared_ptr<DescriptorSet>>	m_descriptorSets;
	std::shared_ptr<Material>					m_pMaterial;
	std::vector<uint32_t>						m_materialVariables;
	std::vector<std::shared_ptr<Image>>			m_textures;
	uint32_t									m_renderMask = 0xffffffff;

	friend class Material;
};