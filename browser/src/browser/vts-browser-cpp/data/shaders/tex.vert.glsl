#version 440

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUvInternal;
layout(location = 2) in vec2 inUvExternal;

layout(location = 0) uniform mat4 uniMvp;
layout(location = 4) uniform mat3 uniUvMat;
layout(location = 8) uniform int uniUvMode;

out vec2 varUvs;

void main()
{
	gl_Position = uniMvp * vec4(inPosition, 1.0);
	switch (uniUvMode)
	{
	case 0:
		varUvs = vec2(uniUvMat * vec3(inUvInternal, 1.0));
		break;
	case 1:
		varUvs = vec2(uniUvMat * vec3(inUvExternal, 1.0));
		break;
	}
}
