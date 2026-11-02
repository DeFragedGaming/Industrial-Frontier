#version 330 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vTerrainColor;

out vec4 FragColor;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uViewPosition;

uniform float uLightIntensity;
uniform float uAmbientIntensity;

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDirection = normalize(-uLightDirection);

    float diffuseStrength = max(
        dot(normal, lightDirection),
        0.0
    );

    vec3 ambient =
        uAmbientColor *
        uAmbientIntensity;

    vec3 diffuse =
        uLightColor *
        diffuseStrength *
        uLightIntensity;

    vec3 lighting = ambient + diffuse;

    vec3 finalColor = vTerrainColor * lighting;

    FragColor = vec4(finalColor, 1.0);
}