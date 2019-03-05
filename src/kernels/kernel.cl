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

__kernel void pre_rays(read_only image2d_t positions, constant light* vpls, const uint noOfVPLS, const uint pwidth, global ray* rays){
	const uint x = get_global_id(0);
	const uint y = get_global_id(1);
	const uint v = get_global_id(2);
	const int i = ((y*pwidth + x) * noOfVPLS) + v;
	const float3 pos = read_imagef(positions, sampler, (int2)(x, y)).xyz;
	rays[i].o = (float4) (vpls[v].position, length(pos - vpls[v].position) - 0.01);
	rays[i].d = (float4) (normalize(pos - vpls[v].position), 0.f);
	rays[i].doBackfaceCulling = 0;
}
__kernel void post_rays(constant ray* rays, constant intersection* isects, const uint noOfVPLS, const uint pwidth, write_only image2d_array_t vpl_masks){
	const uint x = get_global_id(0);
	const uint y = get_global_id(1);
	const uint v = get_global_id(2);
	const int i = ((y*pwidth + x) * noOfVPLS) + v;
	//long expected = rays[i].o.w;
	//long actual = isects[i].uvwt.w;
	if(isects[i].prim_id == -1)
		write_imagef(vpl_masks, (int4)(x, y, v, 0), (float4)(1));
	else
		write_imagef(vpl_masks, (int4)(x, y, v, 0), (float4)(0));
	//write_imagef(vpl_mask, (int2)(x, y), (float4)(isects[i].uvwt));
	//write_imagef(vpl_masks, (int4)(x, y, v, 0), (float4)(1));
	
}
