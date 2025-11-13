// Vertex shader (GLSL)
#version 450

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform PushConstants {
	float yaw;
	float pitch;
	float roll;
	float width;
	float height;
	float distance;
	float modelX;
	float modelY;
	float modelZ;
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
	// Camera rotations: yaw (Y-axis), then pitch (X-axis), then roll (Z-axis)
	mat4 rotY = rotationMatrix(vec3(0.0, 1.0, 0.0), pc.yaw);
	mat4 rotX = rotationMatrix(vec3(1.0, 0.0, 0.0), pc.pitch);
	mat4 rotZ = rotationMatrix(vec3(0.0, 0.0, 1.0), pc.roll);
	mat4 cameraRotation = rotZ * rotX * rotY;
	mat4 viewRotation = transpose(cameraRotation); // inverse for orthonormal rotation matrix

	// Model position in world space
	vec4 worldPos = vec4(inPos + vec3(pc.modelX, pc.modelY, pc.modelZ), 1.0);

	// Camera positioned along negative Z axis by distance
	vec3 cameraPos = vec3(0.0, 0.0, -pc.distance);
	vec4 viewPos = vec4(worldPos.xyz - cameraPos, 1.0);
	viewPos = viewRotation * viewPos;
	
	// Perspective projection using vertical FOV of 60 degrees
	float aspect = pc.width > 0.0 ? pc.width / pc.height : 1.0;
	float nearPlane = 0.1;
	float farPlane = 50.0;
	float fov = radians(60.0);
	float f = 1.0 / tan(fov * 0.5);
	
	mat4 proj = mat4(
		vec4(f / aspect, 0.0, 0.0, 0.0),
		vec4(0.0, -f, 0.0, 0.0), // Vulkan NDC has inverted Y
		vec4(0.0, 0.0, -(farPlane + nearPlane) / (farPlane - nearPlane), -1.0),
		vec4(0.0, 0.0, -(2.0 * farPlane * nearPlane) / (farPlane - nearPlane), 0.0)
	);
	
	gl_Position = proj * viewPos;
}


