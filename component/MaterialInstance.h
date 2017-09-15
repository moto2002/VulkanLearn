#pragma once
#include "../Base/BaseComponent.h"

class DescriptorSet;
class DescriptorPool;
class Material;

class MaterialInstance : public BaseComponent
{
public:
	std::vector<std::shared_ptr<DescriptorSet>> GetDescriptorSets() const { return m_descriptorSets; }
	std::shared_ptr<DescriptorSet> GetDescriptorSet(uint32_t index) const { return m_descriptorSets[index]; }
	std::shared_ptr<Material> GetMaterial() const { return m_pMaterial; }
	uint32_t GetRenderMask() const { return m_renderMask; }
	void SetRenderMask(uint32_t renderMask) { m_renderMask = renderMask; }

protected:
	bool Init(const std::shared_ptr<MaterialInstance>& pMaterialInstance);

protected:
	std::vector<std::shared_ptr<DescriptorSet>>	m_descriptorSets;
	std::shared_ptr<Material>					m_pMaterial;
	uint32_t									m_renderMask = 0xffffffff;

	friend class Material;
};