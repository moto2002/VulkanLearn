#include "PlanetGenerator.h"
#include "MeshRenderer.h"
#include "../Base/BaseObject.h"
#include "../class/PlanetGeoDataManager.h"
#include "../Maths/Plane.h"
#include "../Maths/MathUtil.h"
#include "../scene/SceneGenerator.h"
#include "../class/UniformData.h"
#include "PhysicalCamera.h"

DEFINITE_CLASS_RTTI(PlanetGenerator, BaseComponent);

std::shared_ptr<PlanetGenerator> PlanetGenerator::Create(const std::shared_ptr<PhysicalCamera>& pCamera)
{
	std::shared_ptr<PlanetGenerator> pPlanetGenerator = std::make_shared<PlanetGenerator>();
	if (pPlanetGenerator.get() && pPlanetGenerator->Init(pPlanetGenerator, pCamera))
		return pPlanetGenerator;
	return nullptr;
}

bool PlanetGenerator::Init(const std::shared_ptr<PlanetGenerator>& pSelf, const std::shared_ptr<PhysicalCamera>& pCamera)
{
	if (!BaseComponent::Init(pSelf))
		return false;

	m_pCamera = pCamera;

	float ratio = (1.0f + sqrt(5.0f)) / 2.0f;
	float scale = 1.0f / Vector2f(ratio, 1.0f).Length();
	ratio *= scale;

	Vector3f vertices[] =
	{
		{ ratio, 0, -scale },			//rf 0
		{ -ratio, 0, -scale },			//lf 1
		{ ratio, 0, scale },			//rb 2
		{ -ratio, 0, scale },			//lb 3

		{ 0, -scale, ratio },			//db 4
		{ 0, -scale, -ratio },			//df 5
		{ 0, scale, ratio },			//ub 6
		{ 0, scale, -ratio },			//uf 7

		{ -scale, ratio, 0 },			//lu 8
		{ -scale, -ratio, 0 },			//ld 9
		{ scale, ratio, 0 },			//ru 10
		{ scale, -ratio, 0 }			//rd 11
	};

	for (uint32_t i = 0; i < 12; i++)
	{
		vertices[i].Normalize();
	}

	memcpy_s(&m_icosahedronVertices, sizeof(m_icosahedronVertices), &vertices, sizeof(vertices));

	uint32_t indices[20 * 3] =
	{
		1, 3, 8,
		3, 1, 9,
		0, 10, 2,
		2, 11, 0,

		5, 7, 0,
		7, 5, 1,
		4, 2, 6,
		6, 3, 4,

		9, 11, 4,
		11, 9, 5,
		8, 6, 10,
		10, 7, 8,

		1, 8, 7,
		5, 9, 1,
		0, 7, 10,
		5, 0, 11,

		3, 6, 8,
		4, 3, 9,
		2, 10, 6,
		4, 11, 2
	};

	memcpy_s(&m_icosahedronIndices, sizeof(m_icosahedronIndices), &indices, sizeof(indices));

	Vector3f a = vertices[indices[0]];
	Vector3f b = vertices[indices[1]];
	Vector3f c = vertices[indices[2]];
	Vector3f center = (a + b + c) / 3.0f;
	center.Normalize();

	float cosin_a_center = a * center;

	// cosin_a_center = r / h, r = 1(local length)
	float height_level_0 = 1 / cosin_a_center;
	
	m_heightLUT.push_back(height_level_0);
	for (uint32_t i = 1; i < MAX_LEVEL + 1; i++)
	{
		// Next level vertices
		Vector3f A = (b + c) * 0.5f;
		Vector3f B = (a + c) * 0.5f;
		Vector3f C = (a + b) * 0.5f;
		A.Normalize();

		float cosin_A_center = A * center;
		float height = 1 / cosin_A_center;
		m_heightLUT.push_back(height);

		a = A;
		b = B;
		c = C;
	}

	return true;
}

void PlanetGenerator::Start()
{
	m_pMeshRenderer = GetComponent<MeshRenderer>();

	float size = (m_icosahedronVertices[m_icosahedronIndices[0]] - m_icosahedronVertices[m_icosahedronIndices[1]]).Length();
	float frac = tanf(UniformData::GetInstance()->GetGlobalUniforms()->GetMainCameraHorizontalFOV() * TRIANGLE_SCREEN_SIZE / UniformData::GetInstance()->GetGlobalUniforms()->GetGameWindowSize().x);
	for (uint32_t i = 0; i < MAX_LEVEL + 1; i++)
	{
		m_distanceLUT.push_back(size / frac);
		size *= 0.5f;
	}
	//ASSERTION(m_pMeshRenderer != nullptr);
}

PlanetGenerator::CullState PlanetGenerator::FrustumCull(const Vector3f& a, const Vector3f& b, const Vector3f& c, float height)
{
	CullState state = CullState::DIVIDE;
	for (uint32_t i = 0; i < m_cameraFrustumLocal.FrustumFace_COUNT; i++)
	{
		uint32_t outsideCount = 0;
		outsideCount += m_cameraFrustumLocal.planes[i].PlaneTest(a) > 0 ? 0 : 1;
		outsideCount += m_cameraFrustumLocal.planes[i].PlaneTest(b) > 0 ? 0 : 1;
		outsideCount += m_cameraFrustumLocal.planes[i].PlaneTest(c) > 0 ? 0 : 1;

		if (outsideCount == 3)
		{
			outsideCount += m_cameraFrustumLocal.planes[i].PlaneTest(a * height) > 0 ? 0 : 1;
			outsideCount += m_cameraFrustumLocal.planes[i].PlaneTest(b * height) > 0 ? 0 : 1;
			outsideCount += m_cameraFrustumLocal.planes[i].PlaneTest(c * height) > 0 ? 0 : 1;

			if (outsideCount == 6)
				return CullState::CULL;
			else
				state = CullState::CULL_DIVIDE;
		}
		else if (outsideCount > 0)
			state = CullState::CULL_DIVIDE;
	}

	return state;
}

void PlanetGenerator::SubDivide(uint32_t currentLevel, CullState state, const Vector3f& a, const Vector3f& b, const Vector3f& c, IcoTriangle*& pOutputTriangles)
{
	// Only perform frustum cull if state is CULL_DIVIDE, as it intersects the volumn
	if (state == CullState::CULL_DIVIDE)
	{
		// Frustum cull
		state = FrustumCull(a, b, c, m_heightLUT[currentLevel]);

		// Early quit if triangle is totally outside of the volumn
		if (state == CullState::CULL)
			return;

		// Whatever has left should be either CULL_DIVIDE or DIVIDE
		// This state will be passed on to the next sub divide level
	}

	// vector 0 represents triangle normal
	// vector 1 represents vector from camera position to triangle center
	m_utilityVector0 = a;
	m_utilityVector0 += b;
	m_utilityVector0 += c;

	m_utilityVector1 = m_utilityVector0;
	m_utilityVector1 /= 3.0f;				// Get triangle center
	m_utilityVector1 -= m_cameraPosLocal;	// Get vector from camera to triangle center

	m_utilityVector0.Normalize();
	m_utilityVector1.Normalize();
	
	if (m_utilityVector1 * m_utilityVector0 > 0.4)
		return;

	float distA = (m_cameraPosLocal - a).Length();
	float distB = (m_cameraPosLocal - b).Length();
	float distC = (m_cameraPosLocal - c).Length();

	float minDist = std::fminf(std::fminf(distA, distB), distC);

	if (m_distanceLUT[currentLevel] <= minDist || currentLevel == MAX_LEVEL)
	{
		pOutputTriangles->a = a;
		pOutputTriangles->b = b;
		pOutputTriangles->c = c;
		pOutputTriangles++;
		return;
	}

	Vector3f A, B, C;
	SceneGenerator::GetInstance()->SubDivideTriangle(a, b, c, A, B, C);
	A.Normalize();
	B.Normalize();
	C.Normalize();

	SubDivide(currentLevel + 1, state, a, C, B, pOutputTriangles);
	SubDivide(currentLevel + 1, state, C, b, A, pOutputTriangles);
	SubDivide(currentLevel + 1, state, B, A, c, pOutputTriangles);
	SubDivide(currentLevel + 1, state, A, B, C, pOutputTriangles);
}

void PlanetGenerator::OnPreRender()
{
	if (m_toggleCameraInfoUpdate)
	{
		// Transform from world space to planet local space
		m_utilityTransfrom = GetBaseObject()->GetCachedWorldTransform();
		m_utilityTransfrom.Inverse();

		m_cameraPosLocal = m_utilityTransfrom.TransformAsPoint(m_pCamera->GetBaseObject()->GetCachedWorldPosition());

		// Transfrom from camera local space to world space, and then to planet local space
		m_utilityTransfrom *= m_pCamera->GetBaseObject()->GetCachedWorldTransform();	// from camera local 2 world

		m_cameraFrustumLocal = m_pCamera->GetCameraFrustum();
		m_cameraFrustumLocal.Transform(m_utilityTransfrom);
	}

	uint32_t offsetInBytes;

	IcoTriangle* pTriangles = (IcoTriangle*)PlanetGeoDataManager::GetInstance()->AcquireDataPtr(offsetInBytes);
	uint8_t* startPtr = (uint8_t*)pTriangles;

	for (uint32_t i = 0; i < 20; i++)
	{
		SubDivide(0, CullState::CULL_DIVIDE, m_icosahedronVertices[m_icosahedronIndices[i * 3]], m_icosahedronVertices[m_icosahedronIndices[i * 3 + 1]], m_icosahedronVertices[m_icosahedronIndices[i * 3 + 2]], pTriangles);
	}
	uint32_t updatedSize = (uint8_t*)pTriangles - startPtr;

	PlanetGeoDataManager::GetInstance()->FinishDataUpdate(updatedSize);
	
	if (m_pMeshRenderer != nullptr)
	{
		m_pMeshRenderer->SetStartInstance(offsetInBytes / sizeof(IcoTriangle));
		m_pMeshRenderer->SetInstanceCount(updatedSize / sizeof(IcoTriangle));
	}
}