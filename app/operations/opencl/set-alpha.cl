__kernel void kernel_gimp_operation_set_alpha_with_aux (__global const float4 *in,
                                                        __global const float  *aux,
                                                        __global       float4 *out,
                                                        float value)
{
  int    gid  = get_global_id(0);
  float4 in_v = in[gid];

  in_v.w = value * aux[gid];

  out[gid]  = in_v;
}

__kernel void kernel_gimp_operation_set_alpha (__global const float4 *in,
                                               __global       float4 *out,
                                               float value)
{
  int    gid  = get_global_id(0);
  float4 in_v = in[gid];

  in_v.w = value;

  out[gid] = in_v;
}
