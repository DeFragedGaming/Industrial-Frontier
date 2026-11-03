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

const float SPECULAR_STRENGTH = 0.22;
const float SPECULAR_SHININESS = 32.0;

const float FOG_START = 100.0;
const float FOG_END = 260.0;

const vec3 FOG_COLOR = vec3(0.55, 0.62, 0.68);

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDirection = normalize(-uLightDirection);
    vec3 viewDirection = normalize(uViewPosition - vWorldPosition);

    float diffuseStrength = max(
        dot(normal, lightDirection),
        0.0
    );

    vec3 ambient = uAmbientColor * uAmbientIntensity;

    vec3 diffuse =
        uLightColor *
        diffuseStrength *
        uLightIntensity;

    vec3 halfwayDirection = normalize(
        lightDirection + viewDirection
    );

    float specularAmount = pow(
        max(dot(normal, halfwayDirection), 0.0),
        SPECULAR_SHININESS
    );

    vec3 specular =
        uLightColor *
        specularAmount *
        SPECULAR_STRENGTH *
        uLightIntensity;

    vec3 lighting = ambient + diffuse;

    vec3 terrainColor = vTerrainColor * lighting;
    terrainColor += specular;

    float viewDistance = distance(
        uViewPosition,
        vWorldPosition
    );

    float fogFactor = clamp(
        (viewDistance - FOG_START) /
        (FOG_END - FOG_START),
        0.0,
        1.0
    );

    fogFactor = smoothstep(0.0, 1.0, fogFactor);

    vec3 finalColor = mix(
        terrainColor,
        FOG_COLOR,
        fogFactor
    );

    FragColor = vec4(finalColor, 1.0);
}