#define TS 32
Texture2D<min16uint4> matrixA : register(t0);
Texture2D<min16uint4> matrixB : register(t1);
RWTexture2D<unorm float4> matrixC : register(u0);

cbuffer randomRowCol : register(b0)
{
	uint randomRow;
	uint randomCol;
};

[numthreads(TS, TS, 1)]
void main(uint3 Gid : SV_GroupID, uint3 LocalId : SV_GroupThreadID)
{
	uint globalRow = randomRow + Gid.x * uint(32) + LocalId.x;
	uint globalCol = randomCol + Gid.y * uint(32) + LocalId.y;
	uint outputGlobalRow = Gid.x * uint(32) + LocalId.x;
	uint outputGlobalCol = Gid.y * uint(32) + LocalId.y;
	matrixC[uint2(outputGlobalRow, outputGlobalCol)] = (matrixA[uint2(globalRow, globalCol)] + matrixB[uint2(globalRow, globalCol)]) / float(256);
}