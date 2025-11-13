// Fragment shader (GLSL)
#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(location=0) out vec4 outColor;

void main() {
	vec4 sampled = texture(texSampler, fragUV);
	outColor = sampled;
}


