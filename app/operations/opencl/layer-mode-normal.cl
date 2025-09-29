__kernel void kernel_gimp_operation_normal_union_with_mask (__global const float4 *in,
                                                            __global const float4 *layer,
                                                            __global const float  *mask,
                                                            __global       float4 *out,
                                                            float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity * mask[gid];
  out_v.w = layer_alpha + in_v.w - layer_alpha * in_v.w;

  float opaque = (out_v.w == 0.f);
  float translucent = 1.f - opaque;

  float layer_weight = (opaque * 0.f) + (translucent * (layer_alpha / out_v.w));
  float in_weight    = (opaque * 0.f) + (translucent * (1.f - layer_weight));

  out_v.x = (opaque * in_v.x) + (translucent * (layer_v.x * layer_weight + in_v.x * in_weight));
  out_v.y = (opaque * in_v.y) + (translucent * (layer_v.y * layer_weight + in_v.y * in_weight));
  out_v.z = (opaque * in_v.z) + (translucent * (layer_v.z * layer_weight + in_v.z * in_weight));

  out[gid] = out_v;
}

__kernel void kernel_gimp_operation_normal_union (__global const float4 *in,
                                                  __global const float4 *layer,
                                                  __global       float4 *out,
                                                  float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity;
  out_v.w = layer_alpha + in_v.w - layer_alpha * in_v.w;

  float opaque = (out_v.w == 0.f);
  float translucent = 1.f - opaque;

  float layer_weight = (opaque * 0.f) + (translucent * (layer_alpha / out_v.w));
  float in_weight    = (opaque * 0.f) + (translucent * (1.f - layer_weight));

  out_v.x = (opaque * in_v.x) + (translucent * (layer_v.x * layer_weight + in_v.x * in_weight));
  out_v.y = (opaque * in_v.y) + (translucent * (layer_v.y * layer_weight + in_v.y * in_weight));
  out_v.z = (opaque * in_v.z) + (translucent * (layer_v.z * layer_weight + in_v.z * in_weight));

  out[gid] = out_v;
}

__kernel void kernel_gimp_operation_normal_clip_to_backdrop_with_mask (__global const float4 *in,
                                                                       __global const float4 *layer,
                                                                       __global const float  *mask,
                                                                       __global       float4 *out,
                                                                       float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity * mask[gid];

  float opaque = (out_v.w == 0.f);
  float translucent = 1.f - opaque;

  out_v.x = (opaque * in_v.x) + (translucent * (in_v.x + (layer_v.x - in_v.x) * layer_alpha));
  out_v.y = (opaque * in_v.y) + (translucent * (in_v.y + (layer_v.y - in_v.y) * layer_alpha));
  out_v.z = (opaque * in_v.z) + (translucent * (in_v.z + (layer_v.z - in_v.z) * layer_alpha));

  out[gid] = out_v;
}

__kernel void kernel_gimp_operation_normal_clip_to_backdrop (__global const float4 *in,
                                                             __global const float4 *layer,
                                                             __global       float4 *out,
                                                             float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity;

  float opaque = (out_v.w == 0.f);
  float translucent = 1.f - opaque;

  out_v.x = (opaque * in_v.x) + (translucent * (in_v.x + (layer_v.x - in_v.x) * layer_alpha));
  out_v.y = (opaque * in_v.y) + (translucent * (in_v.y + (layer_v.y - in_v.y) * layer_alpha));
  out_v.z = (opaque * in_v.z) + (translucent * (in_v.z + (layer_v.z - in_v.z) * layer_alpha));

  out[gid] = out_v;
}

__kernel void kernel_gimp_operation_normal_clip_to_layer_with_mask (__global const float4 *in,
                                                                    __global const float4 *layer,
                                                                    __global const float  *mask,
                                                                    __global       float4 *out,
                                                                    float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity * mask[gid];
  out_v.w           = layer_alpha;

  if (out_v.w != 0.f)
    {
      out_v.x = layer_v.x;
      out_v.y = layer_v.y;
      out_v.z = layer_v.z;
    }

  out[gid] = out_v;
}


__kernel void kernel_gimp_operation_normal_clip_to_layer (__global const float4 *in,
                                                          __global const float4 *layer,
                                                          __global       float4 *out,
                                                          float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity;
  out_v.w           = layer_alpha;

  if (out_v.w != 0.f)
    {
      out_v.x = layer_v.x;
      out_v.y = layer_v.y;
      out_v.z = layer_v.z;
    }

  out[gid] = out_v;
}

__kernel void kernel_gimp_operation_normal_intersection_with_mask (__global const float4 *in,
                                                                   __global const float4 *layer,
                                                                   __global const float  *mask,
                                                                   __global       float4 *out,
                                                                   float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in_v;

  float layer_alpha = layer_v.w * opacity * mask[gid];
  out_v.w           = in_v.w * layer_alpha;

  if (out_v.w != 0.f)
    {
      out_v.x = layer_v.x;
      out_v.y = layer_v.y;
      out_v.z = layer_v.z;
    }

  out[gid] = out_v;

}

__kernel void kernel_gimp_operation_normal_intersection (__global const float4 *in,
                                                         __global const float4 *layer,
                                                         __global       float4 *out,
                                                         float opacity)
{
  int gid = get_global_id (0);

  float4 in_v    = in[gid];
  float4 layer_v = layer[gid];
  float4 out_v   = in[gid];

  float layer_alpha = layer_v.w * opacity;
  out_v.w           = in_v.w * layer_alpha;

  if (out_v.w != 0.f)
    {
      out_v.x = layer_v.x;
      out_v.y = layer_v.y;
      out_v.z = layer_v.z;
    }

  out[gid] = out_v;
}
