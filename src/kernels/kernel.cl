typedef struct{
  float4 o;
  float4 d;
  int2 extra;
  int doBackfaceCulling;
  int padding;
} ray;

typedef struct{
    int shape_id;
    int prim_id;
    int2 padding;
    float4 uvwt;
} intersection;

typedef struct{
  float3 position;
  float3 diffuse;
  float3 specular;
} light;

const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel void pre_rays(read_only image2d_t positions, constant light* vpls, const uint vplsPerPixel, const float realVPP, const uint pwidth, const uint iss, const uint ihi, const uint ihs, global ray* rays){
	const uint x = get_global_id(0) / realVPP;
	const uint y = get_global_id(1) / realVPP;
	const uint v = get_global_id(2);
	const uint pv = (ihs * vplsPerPixel * (((y % iss) * iss) + (x % iss)) + v) + ihi;
	const int i = ((y*pwidth + x) * vplsPerPixel) + v;
	const float3 pos = read_imagef(positions, sampler, (int2)(x, y)).xyz;
	rays[i].o = (float4) (vpls[pv].position, length(pos - vpls[pv].position) - 0.001);
	rays[i].d = (float4) (normalize(pos - vpls[pv].position), 0.f);
	//rays[i].doBackfaceCulling = 1;
}
__kernel void post_rays(constant ray* rays, constant int* occlus, const uint vplsPerPixel, const float realVPP, const uint pwidth, const uint iss, const uint ihi, const uint ihs, const write_only image2d_array_t vpl_masks){
	const uint x = get_global_id(0) / realVPP;
	const uint y = get_global_id(1) / realVPP;
	const uint v = get_global_id(2);
	const uint pv = (ihs * vplsPerPixel * (((y % iss) * iss) + (x % iss)) + v) + ihi;
	const int i = ((y*pwidth + x) * vplsPerPixel) + v;
	if(occlus[i] == -1)
		write_imagef(vpl_masks, (int4)(x, y, pv, 0), (float4)(1));
	else
		write_imagef(vpl_masks, (int4)(x, y, pv, 0), (float4)(0));
	
}
