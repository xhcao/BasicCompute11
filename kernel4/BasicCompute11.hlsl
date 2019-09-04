#define M 1280
#define N 1280
#define K 1280
#define TS 32

#define WIDTH 4
#if WIDTH == 1
typedef float floatX;
#elif WIDTH == 2
typedef float2 floatX;
#elif WIDTH == 4
typedef float4 floatX;
#endif

Texture2D<float> matrixA : register(t0);
Texture2D<float> matrixB : register(t1);
RWTexture2D<float> matrixC : register(u0);

groupshared float Asub[TS][TS/WIDTH];
groupshared float Bsub[TS][TS/WIDTH];

[numthreads(TS, TS, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 LocalId : SV_GroupThreadID)
{
	uint row = LocalId.x;
	uint col = LocalId.y;
	uint globalRow = TS * Gid.x + row;
	uint globalCol = TS * Gid.y + col;

#if WIDTH == 1
	floatX acc = 0.0f;
#elif WIDTH == 2
	floatX acc = { 0.0f, 0.0f };
#elif WIDTH == 4
	floatX acc = { 0.0f, 0.0f, 0.0f, 0.0f };
#endif

	uint numTiles = K / TS;
	for (uint t = 0; t < numTiles; t++)
	{
		uint tiledRow = (TS/WIDTH) * t + row;
		uint tiledCol = TS * t + col;
		Asub[col][row] = matrixA[uint2(tiledCol, globalRow)];
		Bsub[col][row] = matrixB[uint2(globalCol, tiledRow)];
		GroupMemoryBarrierWithGroupSync();

		for (int k = 0; k < TS; k++)
		{
			acc += Asub[k][row] * Bsub[col][k];
		}

		GroupMemoryBarrierWithGroupSync();
	}

	matrixC[uint2(globalCol, globalRow)] = acc;
}