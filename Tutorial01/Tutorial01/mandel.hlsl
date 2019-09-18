#define UNROLL 32

RWTexture2D<unorm float4> output : register (u0);

cbuffer cbCSMandel : register(b0)
{

	float a0, b0, da, db;
	float  ja0, jb0; // julia set point

	int max_iterations;
	bool julia;  // julia or mandel
	int   cycle;
};





[numthreads(16, 16, 1)]
//****************************************************************************
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
//****************************************************************************
{
	int counter = max_iterations & (~(UNROLL - 1));

	min16float u, v;
	u = (min16float)(a0 + da * DTid.x);
	v = (min16float)(b0 + db * DTid.y);


	min16float a, b;
	a = (julia) ? (min16float) ja0 : u;
	b = (julia) ? (min16float) jb0 : v;

	bool inside;
	min16float ur, vr, t;
	ur = u;
	vr = v;

	b = b * 0.5f;

	do
	{
		[unroll]
		for (int i = 0; i < UNROLL / 2; i++)
		{
			t = u * u + a - v * v;
			v = 2 * (u*v + b);
			u = t * t + a - v * v;
			v = 2 * (t*v + b);
		}

		inside = u * u + v * v < 4.0f;
		counter -= (inside) ? UNROLL : 0;
		ur = (inside) ? u : ur;
		vr = (inside) ? v : vr;
	} while (inside && counter != 0);


	u = ur;
	v = vr;
	do
	{
		t = u * u + a - v * v;
		v = 2 * (u*v + b);
		u = t;
		inside = (u*u + v * v < 4.0f) && (counter != 0);
		counter -= (inside) ? 1 : 0;
	} while (inside);


	float4 color;

	float c = (float)(counter + cycle) * 2 * 3.1416f / 256.0f;  // color cycle
	color.r = (1 + cos(c)) / 2;
	color.g = (1 + cos(2 * c + 2 * 3.1416f / 3)) / 2;
	color.b = (1 + cos(c - 2 * 3.1416f / 3)) / 2;
	color.a = 1;


	output[DTid.xy] = (counter == 0) ? 0 : color;

}