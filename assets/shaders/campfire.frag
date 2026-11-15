#version 330 core
in vec3 vColor;
out vec4 FragColor;
uniform float uOpacity;
void main()
{
    FragColor = vec4(vColor, uOpacity);
}
