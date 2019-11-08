#define TS 32
Texture2D<uint4> matrixA : register(t0);
Texture2D<uint4> matrixB : register(t1);
RWTexture2D<unorm float4> matrixC : register(u0);

[numthreads(TS, TS, 1)]
void main(uint3 Gid : SV_GroupID, uint3 LocalId : SV_GroupThreadID)
{
	uint globalRow = Gid.x * uint(32) + LocalId.x;
	uint globalCol = Gid.y * uint(32) + LocalId.y;
	matrixC[uint2(globalRow, globalCol)] = (matrixA[uint2(globalRow, globalCol)] + matrixB[uint2(globalRow, globalCol)]) / float(512);
}