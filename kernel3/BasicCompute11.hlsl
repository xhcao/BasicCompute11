#define M 1280
#define N 1280
#define K 1280
#define TS 32
#define WPT 8
#define RTS 4
Texture2D<float> matrixA : register(t0);
Texture2D<float> matrixB : register(t1);
RWTexture2D<float> matrixC : register(u0);

groupshared float Asub[TS][TS];
groupshared float Bsub[TS][TS];

[numthreads(TS, RTS, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 LocalId : SV_GroupThreadID)
{
	uint row = LocalId.x;
	uint col = LocalId.y;
	uint globalRow = TS * Gid.x + row;
	uint globalCol = TS * Gid.y + col;

	float acc[WPT];
	for (int w = 0; w < WPT; w++) {
		acc[w] = 0.0f;
	}

	uint numTiles = K / TS;
	for (uint t = 0; t < numTiles; t++)
	{
		for (int w = 0; w < WPT; w++) {
			uint tiledRow = TS * t + row;
			uint tiledCol = TS * t + col;
			Asub[col + w * RTS][row] = matrixA[uint2(tiledCol + w * RTS, globalRow)];
			Bsub[col + w * RTS][row] = matrixB[uint2(globalCol + w * RTS, tiledRow)];
		}
		
		GroupMemoryBarrierWithGroupSync();

		for (int k = 0; k < TS; k++)
		{
			for (int w = 0; w < WPT; w++) {
				acc[w] += Asub[k][row] * Bsub[col + w * RTS][k];
			}
		}

		GroupMemoryBarrierWithGroupSync();
	}

	for (int w = 0; w < WPT; w++) {
		matrixC[uint2(globalCol + w * RTS, globalRow)] = acc[w];
	}
}