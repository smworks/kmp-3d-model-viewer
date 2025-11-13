// Vertex shader (GLSL)
#version 450

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform PushConstants {
	float yaw;
	float pitch;
	float roll;
} pc;

mat4 rotationMatrix(vec3 axis, float angle) {
	float c = cos(angle);
	float s = sin(angle);
	float t = 1.0 - c;
	
	vec3 n = normalize(axis);
	float x = n.x, y = n.y, z = n.z;
	
	return mat4(
		t*x*x + c,    t*x*y - s*z,  t*x*z + s*y,  0.0,
		t*x*y + s*z,  t*y*y + c,    t*y*z - s*x,  0.0,
		t*x*z - s*y,  t*y*z + s*x,  t*z*z + c,    0.0,
		0.0,          0.0,          0.0,          1.0
	);
}

void main() {
	// Scale up the cube for better visibility (2x scale)
	vec3 scaledPos = inPos * 2.0;
	
	// Apply rotations: first yaw (Y-axis), then pitch (X-axis), then roll (Z-axis)
	mat4 rotY = rotationMatrix(vec3(0.0, 1.0, 0.0), pc.yaw);
	mat4 rotX = rotationMatrix(vec3(1.0, 0.0, 0.0), pc.pitch);
	mat4 rotZ = rotationMatrix(vec3(0.0, 0.0, 1.0), pc.roll);
	
	mat4 rotation = rotZ * rotX * rotY;
	vec4 rotatedPos = rotation * vec4(scaledPos, 1.0);
	
	// Simple orthographic projection to NDC [-1, 1]
	// Map from roughly [-2, 2] to [-1, 1] with some margin
	float scale = 0.3; // Make it smaller to fit in view
	gl_Position = vec4(rotatedPos.xy * scale, rotatedPos.z * 0.1, 1.0);
}


