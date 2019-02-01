#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "uniform_layout.h"

layout (set = 3, binding = 2) uniform sampler2D PrefilteredCoC[3];

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outBlurredColor;

int index = int(perFrameData.camDir.a);

static const int kSampleCount = 43;
static const float2 kDiskKernel[kSampleCount] = {
    float2(0,0),
    float2(0.36363637,0),
    float2(0.22672357,0.28430238),
    float2(-0.08091671,0.35451925),
    float2(-0.32762504,0.15777594),
    float2(-0.32762504,-0.15777591),
    float2(-0.08091656,-0.35451928),
    float2(0.22672352,-0.2843024),
    float2(0.6818182,0),
    float2(0.614297,0.29582983),
    float2(0.42510667,0.5330669),
    float2(0.15171885,0.6647236),
    float2(-0.15171883,0.6647236),
    float2(-0.4251068,0.53306687),
    float2(-0.614297,0.29582986),
    float2(-0.6818182,0),
    float2(-0.614297,-0.29582983),
    float2(-0.42510656,-0.53306705),
    float2(-0.15171856,-0.66472363),
    float2(0.1517192,-0.6647235),
    float2(0.4251066,-0.53306705),
    float2(0.614297,-0.29582983),
    float2(1,0),
    float2(0.9555728,0.2947552),
    float2(0.82623875,0.5633201),
    float2(0.6234898,0.7818315),
    float2(0.36534098,0.93087375),
    float2(0.07473,0.9972038),
    float2(-0.22252095,0.9749279),
    float2(-0.50000006,0.8660254),
    float2(-0.73305196,0.6801727),
    float2(-0.90096885,0.43388382),
    float2(-0.98883086,0.14904208),
    float2(-0.9888308,-0.14904249),
    float2(-0.90096885,-0.43388376),
    float2(-0.73305184,-0.6801728),
    float2(-0.4999999,-0.86602545),
    float2(-0.222521,-0.9749279),
    float2(0.07473029,-0.99720377),
    float2(0.36534148,-0.9308736),
    float2(0.6234897,-0.7818316),
    float2(0.8262388,-0.56332),
    float2(0.9555729,-0.29475483),
};

void main() 
{
	vec4 center = texture(PrefilteredCoC[index], inUv);

	vec4 farColor = vec4(0);
	vec4 nearColor = vec4(0);

	for (int i = 0; i < BOKEH_KERNEL_COUNT; i++)
	{
		vec2 offset = BOKEH_KERNEL[i] * globalData.DOFSettings0.x;
		float dist = length(offset);

		offset = vec2(offset.x / globalData.MainCameraSettings0.x, offset.y);
		
		vec4 curSample = texture(PrefilteredCoC[index], inUv + offset).rgba;

		float farCoC = max(min(center.a, curSample.a), 0.0f);

		float margin = globalData.gameWindowSize.z * 4.0f;
		float farWeight = clamp((farCoC - dist + margin) / margin, 0, 1);
		float nearWeight = clamp((-curSample.a - dist + margin) / margin, 0, 1);

		nearWeight *= step(globalData.gameWindowSize.z * 2.0f, -curSample.a);

		farColor += vec4(curSample.rgb, 1.0f) * farWeight;
		nearColor += vec4(curSample.rgb, 1.0f) * nearWeight;
	}

	farColor.rgb /= (farColor.a + float(farColor.a == 0.0f));
	nearColor.rgb /= (nearColor.a + float(nearColor.a == 0.0f));

	farColor.a = smoothstep(globalData.gameWindowSize.z * 2.0f, globalData.gameWindowSize.z * 4.0f, center.a);
	nearColor.a /= float(BOKEH_KERNEL_COUNT);

	float nearCoC = clamp(nearColor.a, 0, 1);
	vec3 CoCColor = mix(farColor.rgb, nearColor.rgb, nearCoC);

	outBlurredColor = vec4(CoCColor, nearCoC);
}