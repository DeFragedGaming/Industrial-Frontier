#version 330 core
in vec3 vPosition;
in vec3 vColor;
in vec3 vFlow;

uniform float uTime;
out vec4 FragColor;

void main()
{
    vec2 direction = normalize(vFlow.xz + vec2(0.00001, 0.0));
    vec2 sideways = vec2(-direction.y, direction.x);
    float speed = max(vFlow.y, 0.02);
    float along = dot(vPosition.xz, direction);
    float across = dot(vPosition.xz, sideways);
    float stream = along * 2.1 - uTime * speed * 4.4;
    float ripple = sin(stream + 0.60 * sin(across * 1.45)) *
        sin(across * 1.36 + stream * 0.34);
    float glint = pow(max(0.0, sin(stream * 0.77 + across * 1.9)), 9.0);
    float slow = 1.0 - smoothstep(0.15, 0.65, speed);
    float lakeShimmer = sin(vPosition.x * 0.55 + uTime * 0.5) *
        sin(vPosition.z * 0.68 - uTime * 0.34);
    vec3 color = vColor * (0.95 + 0.10 * ripple);
    color += vec3(0.14, 0.24, 0.25) * glint * (0.36 + speed * 0.42);
    color += vec3(0.028, 0.055, 0.060) * lakeShimmer * slow;
    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
