#pragma once

#include "IMaterialUniformOperator.h"
#include <gli\gli.hpp>
#include <map>

class Texture2D;
class TextureCube;
class Texture2DArray;
class Image;
class DescriptorSet;

enum InGameTextureType
{
	RGBA8_1024,
	R8_1024,
	InGameTextureTypeCount
};

enum IBLTextureType
{
	RGBA16_1024_SkyBox,
	RGBA16_512_SkyBoxIrradiance,
	RGBA16_512_SkyBoxPrefilterEnv,
	IBLCubeTextureTypeCount,
	RGBA16_512_BRDFLut = 0,
	IBL2DTextureTypeCount,
	IBLTextureTypeCount = IBLCubeTextureTypeCount + IBL2DTextureTypeCount
};

typedef struct _TextureDesc
{
	std::string textureName;
	std::string texturePath;
	std::string textureDescription;
}TextureDesc;

typedef struct _TextureArrayDesc
{
	std::string						textureArrayName;
	std::string						textureArrayDescription;
	std::map<uint32_t, TextureDesc>	textureDescriptions;	// Key: index in texture array
	uint32_t						maxSlotIndex;
	uint32_t						currentEmptySlot;		// Empty slot is available for new texture, and is updated everytime when textureDescriptions changed
	std::shared_ptr<Image>			pTextureArray;
	std::map<std::string, uint32_t> lookupTable;
}TextureArrayDesc;

class GlobalTextures : public SelfRefBase<GlobalTextures>, public IMaterialUniformOperator
{
public:
	const static uint32_t SSAO_RANDOM_ROTATION_COUNT = 16;

public:
	static std::shared_ptr<GlobalTextures> Create();

public:
	void InsertTexture(InGameTextureType type, const TextureDesc& desc, const gli::texture2d& gliTexture2d);
	void InsertScreenSizeTexture(const TextureDesc& desc);
	std::shared_ptr<Image>	GetTextureArray(InGameTextureType type) const { return m_textureDiction[type].pTextureArray; }
	std::shared_ptr<Image>	GetScreenSizeTextureArray() const { return m_screenSizeTextureDiction.pTextureArray; }
	std::shared_ptr<Image> GetIBLTextureCube(IBLTextureType type) const { return m_IBLCubeTextures[type]; }
	std::shared_ptr<Image> GetIBLTexture2D(IBLTextureType type) const { return m_IBL2DTextures[type]; }
	std::shared_ptr<Image> GetTransmittanceTextureDiction(uint32_t planetIndex) const { return m_transmittanceTextureDiction[planetIndex]; }
	std::shared_ptr<Image> GetScatterTextureDiction(uint32_t planetIndex) const { return m_scatterTextureDiction[planetIndex]; }
	std::shared_ptr<Image> GetIrradianceTextureDiction(uint32_t planetIndex) const { return m_irradianceTextureDiction[planetIndex]; }
	std::shared_ptr<Image> GetDeltaIrradiance() const { return m_pDeltaIrradiance; }
	std::shared_ptr<Image> GetDeltaRayleigh() const { return m_pDeltaRayleigh; }
	std::shared_ptr<Image> GetDeltaMie() const { return m_pDeltaMie; }
	std::shared_ptr<Image> GetDeltaScatterDensity() const { return m_pDeltaScatterDensity; }
	std::shared_ptr<Image> GetDeltaMultiScatter() const { return m_pDeltaMultiScatter; }
	void InitIBLTextures(const gli::texture_cube& skyBoxTex);
	bool GetTextureIndex(InGameTextureType type, const std::string& textureName, uint32_t& textureIndex);
	bool GetScreenSizeTextureIndex(const std::string& textureName, uint32_t& textureIndex);

	virtual std::vector<UniformVarList> PrepareUniformVarList() const override;
	uint32_t SetupDescriptorSet(const std::shared_ptr<DescriptorSet>& pDescriptorSet, uint32_t bindingIndex) const override;

protected:
	bool Init(const std::shared_ptr<GlobalTextures>& pSelf);
	void InitTextureDiction();
	void InitScreenSizeTextureDiction();
	void InitIBLTextures();
	void InitIrradianceTexture();
	void InitPrefilterEnvTexture();
	void InitBRDFLUTTexture();
	void InitSSAORandomRotationTexture();
	void InitTransmittanceTextureDiction();
	void InsertTextureDesc(const TextureDesc& desc, TextureArrayDesc& textureArr, uint32_t& emptySlot);
	bool GetTextureIndex(const TextureArrayDesc& textureArr, const std::string& textureName, uint32_t& textureIndex);

protected:
	std::vector<TextureArrayDesc>				m_textureDiction;
	TextureArrayDesc							m_screenSizeTextureDiction;
	std::vector<std::shared_ptr<Image>>			m_IBLCubeTextures;
	std::vector<std::shared_ptr<Image>>			m_IBL2DTextures;
	std::shared_ptr<Image>						m_pSSAORandomRotations;

	std::vector<std::shared_ptr<Image>>			m_transmittanceTextureDiction;
	std::vector<std::shared_ptr<Image>>			m_scatterTextureDiction;
	std::vector<std::shared_ptr<Image>>			m_irradianceTextureDiction;

	// FIXME: Remove this later
	std::shared_ptr<Image>						m_pDeltaIrradiance;
	std::shared_ptr<Image>						m_pDeltaRayleigh;
	std::shared_ptr<Image>						m_pDeltaMie;
	std::shared_ptr<Image>						m_pDeltaScatterDensity;
	std::shared_ptr<Image>						m_pDeltaMultiScatter;
};