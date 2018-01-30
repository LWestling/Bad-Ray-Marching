/*
    If someone hack this, this is very inefficient and more of testing concepts and stuff
*/
#define EPS 0.01

struct PS_IN {
    float4 pos : SV_POSITION;
    float4 test : POSITION;
};

cbuffer Camera : register(b0) {
    float4 camPos;
    float4 camLook;
    float4x4 camMatrix;
    float4x4 viewMatrix;
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

float SDF(Sphere sphere, float3 pos) {
    return length(pos - sphere.pos) - sphere.d;
}

float3 calcLightning(float3 normal, float3 pos) {
    float3 lightPos = float3( 0.f, 0.f, -25.f );
    float3 diff = lightPos - pos;
    return dot(normalize(diff), normal);
}

float SDFCube(Cube cube, float3 pos)
{
    return length(max(abs(pos - cube.pos) - cube.d, 0.f));
}

float4 getNormalSphere(Sphere s, float3 p) {
    return normalize(float4(
        SDF(s, float3(p.x + EPS, p.y, p.z)) - SDF(s, float3(p.x - EPS, p.y, p.z)),
        SDF(s, float3(p.x, p.y + EPS, p.z)) - SDF(s, float3(p.x, p.y - EPS, p.z)),
        SDF(s, float3(p.x, p.y, p.z + EPS)) - SDF(s, float3(p.x, p.y, p.z - EPS)),
        0.f
    ));
}

float4 getNormalCube(Cube c, float3 p) {
    return normalize(float4(
        SDFCube(c, float3(p.x + EPS, p.y, p.z)) - SDFCube(c, float3(p.x - EPS, p.y, p.z)),
        SDFCube(c, float3(p.x, p.y + EPS, p.z)) - SDFCube(c, float3(p.x, p.y - EPS, p.z)),
        SDFCube(c, float3(p.x, p.y, p.z + EPS)) - SDFCube(c, float3(p.x, p.y, p.z - EPS)),
        0.f
    ));
}

#define MAX_STEPS 100
#define START 1

float4 main(PS_IN input) : SV_TARGET
{
    Sphere test, test2, test3;
    test.pos = float3(0.f, 0.f, 25.f);
    test.d = 0.9f;
    test2.pos = float3(1.4f, 0.f, 24.f);
    test2.d = 0.84f;
    test3.pos = float3(0.11f, 0.5f, 25.f);
    test3.d = 1.2f;

    Cube cube;
    cube.pos = float3(0.1f, 0.1f, 25.f);
    cube.d = 0.4f;

    float4x4 VPMatrix = mul(viewMatrix, camMatrix);

    float3 pos = input.test.xyz;
    float3 eye = camPos;
    float3 forward = pos - eye;
    forward = normalize(mul(VPMatrix, float4(normalize(pos), 0.f)));
    

    float dist = 0.f;
    for (int i = 0; i < MAX_STEPS; i++) {
        float3 p = eye + forward * dist;
        float ret = max(SDF(test3, p), max(SDF(test, p), SDF(test2, p)));
        if (ret < EPS)
        {
            return float4(float3(1.f, 0.9f, 0.f) * calcLightning(getNormalSphere(test, p), p), 0.f);
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