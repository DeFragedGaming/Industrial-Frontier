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

const float FOG_START = 100.0;
const float FOG_END = 260.0;
const vec3 FOG_COLOR = vec3(0.55, 0.62, 0.68);

float Hash3(vec3 position)
{
    return fract(sin(dot(position, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float Noise3(vec3 position)
{
    vec3 cell = floor(position);
    vec3 local = fract(position);
    local = local * local * (3.0 - 2.0 * local);

    float n000 = Hash3(cell);
    float n100 = Hash3(cell + vec3(1.0, 0.0, 0.0));
    float n010 = Hash3(cell + vec3(0.0, 1.0, 0.0));
    float n110 = Hash3(cell + vec3(1.0, 1.0, 0.0));
    float n001 = Hash3(cell + vec3(0.0, 0.0, 1.0));
    float n101 = Hash3(cell + vec3(1.0, 0.0, 1.0));
    float n011 = Hash3(cell + vec3(0.0, 1.0, 1.0));
    float n111 = Hash3(cell + vec3(1.0, 1.0, 1.0));

    float bottom = mix(
        mix(n000, n100, local.x),
        mix(n010, n110, local.x),
        local.y
    );
    float top = mix(
        mix(n001, n101, local.x),
        mix(n011, n111, local.x),
        local.y
    );
    return mix(bottom, top, local.z);
}

vec4 GetMaterialWeights(vec3 color)
{
    float snow = smoothstep(0.57, 0.72, dot(color, vec3(0.299, 0.587, 0.114)));
    float grass = smoothstep(0.035, 0.125, color.g - color.r) * (1.0 - snow);
    float clay = smoothstep(0.070, 0.145, color.r - color.b) *
                 (1.0 - grass) * (1.0 - snow);
    float stone = max(0.0, 1.0 - snow - grass - clay);
    return vec4(stone, clay, grass, snow);
}

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDirection = normalize(-uLightDirection);
    vec3 viewDirection = normalize(uViewPosition - vWorldPosition);

    vec4 materials = GetMaterialWeights(vTerrainColor);
    float broad = Noise3(vWorldPosition * 0.38);
    float grain = Hash3(floor(vWorldPosition * 3.5));

    float strata = sin(
        vWorldPosition.y * 6.2 +
        vWorldPosition.x * 0.40 +
        broad * 2.2
    );

    float stoneDetail = 0.83 + broad * 0.25 + (grain - 0.5) * 0.11 + strata * 0.045;
    float clayDetail = 0.89 + broad * 0.19 + (grain - 0.5) * 0.065 + strata * 0.035;
    float grassDetail = 0.87 + broad * 0.26 + (grain - 0.5) * 0.17;
    float snowDetail = 0.94 + broad * 0.10 + (grain - 0.5) * 0.055;

    float detail = dot(materials, vec4(
        stoneDetail,
        clayDetail,
        grassDetail,
        snowDetail
    ));

    vec3 baseColor = vTerrainColor * detail;

    float diffuseStrength = max(dot(normal, lightDirection), 0.0);
    vec3 ambient = uAmbientColor * uAmbientIntensity;
    vec3 diffuse = uLightColor * diffuseStrength * uLightIntensity;

    vec3 halfwayDirection = normalize(lightDirection + viewDirection);
    float shininess = dot(materials, vec4(36.0, 20.0, 10.0, 48.0));
    float specularStrength = dot(materials, vec4(0.12, 0.045, 0.025, 0.15));
    float specularAmount = pow(max(dot(normal, halfwayDirection), 0.0), shininess);

    vec3 terrainColor = baseColor * (ambient + diffuse);
    terrainColor += uLightColor * specularAmount * specularStrength * uLightIntensity;

    float viewDistance = distance(uViewPosition, vWorldPosition);
    float fogFactor = clamp(
        (viewDistance - FOG_START) / (FOG_END - FOG_START),
        0.0,
        1.0
    );
    fogFactor = smoothstep(0.0, 1.0, fogFactor);

    FragColor = vec4(mix(terrainColor, FOG_COLOR, fogFactor), 1.0);
}
