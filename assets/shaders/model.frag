
#version 330 core

in vec3 vNormal;
in vec3 vWorldPosition;
in vec2 vTexCoord;

out vec4 FragColor;

uniform sampler2D uBaseColorTexture;
uniform bool uHasBaseColorTexture;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

void main()
{
    vec4 baseColor = vec4(1.0);

    if (uHasBaseColorTexture)
    {
        baseColor = texture(
            uBaseColorTexture,
            vTexCoord
        );
    }

    if (baseColor.a < 0.5)
    {
        discard;
    }

    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(-uLightDirection);

    float diffuse = max(
        dot(normal, lightDirection),
        0.0
    );

    vec3 lighting =
        uAmbientColor * 0.65 +
        uLightColor * diffuse * 1.15;

    vec3 color = baseColor.rgb * lighting;

    FragColor = vec4(color, baseColor.a);
}
