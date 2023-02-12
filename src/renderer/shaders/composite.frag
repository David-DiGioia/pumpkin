#version 460

layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0, rgba8) uniform image2D raster_image;
layout (set = 0, binding = 1, rgba8) uniform image2D rt_image;

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);

    // For now just output the ray traced image. Later we can composite the raster image with it.
    //out_color = imageLoad(raster_image, coord);
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
}