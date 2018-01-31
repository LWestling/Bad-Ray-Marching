/*
    If someone hack this, this is very inefficient and more of testing concepts and stuff
*/
#define EPS 0.000001

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
    float timeM = fmod(abs(sin(time)), 5.f) * 1.3;
    float disp = sin(timeM * 2.3f * pos.x) * sin(timeM * 2.7f * pos.y) * sin(timeM * 1.6f * pos.z) * timeM * .25f;
    float4 plane = normalize(float4(0.f, 1.f, 0.f, 100.f));
    Cube c = cube1;
    c.pos.x -= timeM * 1.5;

    float dis_t = SDF(test, pos) + disp;
    float dis_t2 = SDF(test2, pos);
    float dis_t3 = SDF(test3, pos);
    float dis_t4 = SDFCube(cube1, pos);

    float dis_plane = SDFPlane(plane, pos);

    return min(dis_plane, max(-dis_t2, max(-dis_t3, max(dis_t, dis_t4))));
   // return max(dis_t4, max(dis_t2, -dis_t)) + disp;
   // return max(-dis_t2, max(dis_t4, dis_t)) + disp;
}
/* end of world mapping */

/* LIGHTNING */
float3 calcLightning(float3 normal, float3 pos) {
    float3 lightPos = float3( 0.f, 0.f, 5.f );
    float3 diff = lightPos - pos;
    return dot(normalize(diff), normal);
}

float4 getNormal(float3 p) {
    return normalize(float4(
        mapWorld(float3(p.x + EPS, p.y, p.z)) - mapWorld(float3(p.x - EPS, p.y, p.z)),
        mapWorld(float3(p.x, p.y + EPS, p.z)) - mapWorld(float3(p.x, p.y - EPS, p.z)),
        mapWorld(float3(p.x, p.y, p.z + EPS)) - mapWorld(float3(p.x, p.y, p.z - EPS)),
        0.f
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
            return float4(float3(1.f, 0.9f, 0.f) * calcLightning(getNormal(p), p), 0.f);
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