typedef struct{
  float4 o;
  float4 d;
  int2 extra;
  float2 padding;
} ray;

typedef struct{
    int shape_id;
    int prim_id;
    int padding1;
    int padding2;
    float4 uvwt;
} intersection;

typedef struct{
  float4 position;
  float4 diffuse;
  float4 specular;
} light;

const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel void init_masks(const write_only image2d_array_t vpl_masks){
	const uint x = get_global_id(0);
	const uint y = get_global_id(1);
	const uint v = get_global_id(2);
	write_imagef(vpl_masks, (int4)(x, y, v, 0), (float4)(0));
}

__kernel void pre_rays(read_only image2d_t positions, constant light* vpls, const uint vplsPerPixel, const float realVPP, const uint pwidth, const uint iss, const uint ihi, const uint ihs, global ray* rays){
	const uint x = get_global_id(0) / realVPP;
	const uint y = get_global_id(1) / realVPP;
	const uint v = get_global_id(2);
	//const uint pv = ihs * (vplsPerPixel * (((y % iss) * iss) + (x % iss)) + v) + ihi;
	const uint pv = vplsPerPixel * (ihs * (((y % iss) * iss) + (x % iss)) + ihi) + v;
	const int i = ((y*pwidth + x) * vplsPerPixel) + v;
	const float3 pos = read_imagef(positions, sampler, (int2)(x, y)).xyz;
	const float3 vpos = vpls[pv].position.xyz;
	//const float3 dpos = vpls[1].position.xyz;
	//if(x + y == 0 && pv == 1)
	//	printf("%f, %f, %f", dpos.x, dpos.y, dpos.z);
	rays[i].o = (float4) (vpos, length(pos - vpos) - 0.001);
	rays[i].d = (float4) (normalize(pos - vpos), 0.f);
    rays[i].extra.x = 0xFFFFFFFF;
	rays[i].extra.y = 0xFFFFFFFF;
}
__kernel void post_rays(constant ray* rays, constant int* occlus, const uint vplsPerPixel, const float realVPP, const uint pwidth, const uint iss, const uint ihi, const uint ihs, constant light* vpls, const write_only image2d_array_t vpl_masks){
	const uint x = get_global_id(0) / realVPP;
	const uint y = get_global_id(1) / realVPP;
	const uint v = get_global_id(2);
	//const uint pv = ihs * (vplsPerPixel * (((y % iss) * iss) + (x % iss)) + v) + ihi;
	const uint pv = vplsPerPixel * (ihs * (((y % iss) * iss) + (x % iss)) + ihi) + v;
	const int i = ((y*pwidth + x) * vplsPerPixel) + v;
	if(occlus[i] == -1)
		write_imagef(vpl_masks, (int4)(x, y, pv, 0), (float4)(1));
	else
		write_imagef(vpl_masks, (int4)(x, y, pv, 0), (float4)(0));
}
