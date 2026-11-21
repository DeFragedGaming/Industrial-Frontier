#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uMVP;
uniform float uTime;

out vec3 vPosition;
out vec3 vColor;
out vec3 vFlow;

void main()
{
    vec2 direction = normalize(aNormal.xz + vec2(0.00001, 0.0));
    float speed = max(aNormal.y, 0.0);
    float phase = dot(aPosition.xz, direction) * 2.4 - uTime * speed * 3.1;
    vec3 position = aPosition;
    position.y += 0.018 * sin(phase) * min(speed, 1.0);
    vPosition = position;
    vColor = aColor;
    vFlow = vec3(direction.x, speed, direction.y);
    gl_Position = uMVP * vec4(position, 1.0);
}
