/*
    If someone hack this, this is very inefficient and more of testing concepts and stuff
*/
#define EPS 0.0001

struct PS_IN {
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

cbuffer Camera : register(b0) {
    float4 camPos;
    float4 camLook;
    float4x4 camMatrix;
    float4x4 viewMatrix;
}

cbuffer Time : register(b1) {
    float time;
    float3 dataThatIsNeededToFilloutAndShiet;
}

struct Sphere {
    float3 pos;
    float d;
};

// Axis bound cube
struct Cube {
    float3 pos;
    float d;
};

static Sphere test = { float3(0.f, 0.f, 25.f), 1.3f };
static Sphere test2 = { float3(-1.4f, 0.f, 24.2f), 0.9f };
static Sphere test3 = { float3(0.f, 0.f, 25.f), 1.1f };

static Cube cube1 = { float3(0.f, 0.f, 25.f), 0.95f };

/* SDF */
float SDF(Sphere sphere, float3 pos) {
    return length(pos - sphere.pos) - sphere.d;
}

float SDFCube(Cube cube, float3 pos)
{
    return length(max(abs(pos - cube.pos) - cube.d, 0.f));
}

float SDFPlane(float4 nor, float3 pos)
{
    return dot(pos, nor.xyz) + nor.w;
}

/* END OF SDF */

/* World mapping */
float mapWorld(float3 pos) {
    float timeM = fmod(abs(sin(time)), 1.f) + 0.f;
    float disp = 0.1f;
    float4 plane = normalize(float4(sin(time + pos.x), 1.f, 0.f, 1.f));
    Cube c = cube1;
    c.pos.x -= timeM * 1.5;

    float dis_t = SDF(test, pos);
    float dis_t2 = SDF(test2, pos);
    float dis_t3 = SDF(test3, pos);
    float dis_t4 = SDFCube(cube1, pos);

    float dis_plane = SDFPlane(plane, pos);

    return max(-dis_t2, max(-dis_t3, max(dis_t, dis_t4)));
   // return max(dis_t4, max(dis_t2, -dis_t)) + disp;
   // return max(-dis_t2, max(dis_t4, dis_t)) + disp;
}
/* end of world mapping */

/* LIGHTNING */
#define SPEC_CONS 3.f
float3 calcLightning(float3 normal, float3 pos) {
    float3 lightPos = float3( 0.f, 0.f, 5.f );
    float3 toLight = normalize(lightPos - pos);
    float toCam = normalize(camPos.xyz - pos);

    float diffuse = dot(toLight, normal);
    float spec = 0.08f * pow(max(dot(toCam, reflect(-toLight, normal)), 0.f), SPEC_CONS);

    return diffuse + spec;
}

float3 getNormal(float3 p) {
    static float2 eps = float2(EPS, 0.f);
    return normalize(float3(
        mapWorld(p + eps.xyy) - mapWorld(p - eps.xyy),
        mapWorld(p + eps.yxy) - mapWorld(p - eps.yxy),
        mapWorld(p + eps.yyx) - mapWorld(p - eps.yyx)
    ));
}
/* END OF LIGTNING*/

#define MAX_STEPS 100
#define START 1

float4 main(PS_IN input) : SV_TARGET
{
    float4x4 VPMatrix = mul(viewMatrix, camMatrix);

    float3 pos = float3(input.uv, -1.f);
    float3 eye = camPos;
    float3 forward = normalize(pos - eye);
    forward = normalize(mul(camMatrix, float4(pos, 0.f))).xyz;

    float dist = 0.f;
    for (int i = 0; i < MAX_STEPS; i++) {
        float3 p = eye + forward * dist;
        float ret = mapWorld(p);
        if (ret < EPS)
        {
            return float4(float3(0.4f, 0.3f, 0.f) * 
            calcLightning(getNormal(p), p), 0.f);
        }
        dist += ret;
    }
    /*
    dist = 0.f;
    for (int i = 0; i < MAX_STEPS; i++) {
        float3 p = pos + forward * dist;
        float ret = SDF(test2, p);
        if (ret < EPS)
        {
            return float4(float3(1.f, 0.f, 0.f) * calcLightning(getNormalSphere(test2, p), p), 0.f);
        }
        dist += ret;
    } */

/*
    dist = 0;
    for (int i = 0; i < MAX_STEPS; i++) {
        float ret = SDF(test2, pos + forward * dist);
        if (ret < test2.d) {
            return float4(calcLightning(getNormal(test2.d, pos), pos + forward * ret), 0.f);
        }
        dist += ret;
    } */

    return float4(0.1f, 0.1f, 0.1f, 0.1f);
}