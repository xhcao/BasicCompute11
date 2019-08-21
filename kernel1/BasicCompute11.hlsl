#define M 1280
#define N 1280
#define K 1280
#define TS 32
Texture2D<float> matrixA : register(t0);
Texture2D<float> matrixB : register(t1);
RWTexture2D<float> matrixC : register(u0);

[numthreads(TS, TS, 1)]
void CSMain(uint3 Gid : SV_DispatchThreadID)
{
	uint row = Gid.x;
	uint col = Gid.y;

	float result = 0.0f;
	for (uint k = 0; k < 1280; k++)
	{
		result += matrixA[uint2(k, row)] * matrixB[uint2(col, k)];
	}

	matrixC[uint2(col, row)] = result;
}