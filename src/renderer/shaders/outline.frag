#version 460

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform OutlinePushConstant{ vec4 outline_color; } constants;

layout (set = 0, binding = 0, r8ui) readonly uniform uimage2D mask_texture;

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);

    // We only draw outline for pixels outside the object.
    if (imageLoad(mask_texture, coord).x == 1) {
        discard;
    }

    int radius = 2;

    for (int x = -radius; x <= radius; ++x)
    {
        for (int y = -radius; y <= radius; ++y)
        {
            if (x == 0 && y == 0) {
                continue;
            }

            uint mask = imageLoad(mask_texture, coord + ivec2(x, y)).x;
            if (mask == 1)
            {
                // Draw outline only when adjacent pixel contains mask.
                out_color = constants.outline_color;
                return;
            }
        }
    }

    discard;
}