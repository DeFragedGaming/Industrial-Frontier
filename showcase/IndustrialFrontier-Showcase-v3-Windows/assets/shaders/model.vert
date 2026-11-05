
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vWorldPosition;
out vec2 vTexCoord;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;

    vNormal = normalize(
        mat3(transpose(inverse(uModel))) * aNormal
    );

    vTexCoord = aTexCoord;

    gl_Position = uMVP * worldPosition;
}
