#version 460

layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0, r8ui) uniform uimage2D mask_texture;

void main()
{
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
    return;

    ivec2 coord = ivec2(gl_FragCoord.xy);

    // We only draw outline for pixels outside the object.
    if (imageLoad(mask_texture, coord).x == 1) {
        return;
    }

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            if (x == 0 && y == 0) {
                continue;
            }

            uint mask = imageLoad(mask_texture, coord + ivec2(x, y)).x;
            if (mask == 1)
            {
                // Draw outline only when adjacent pixel contains mask.
                out_color = vec4(0.8, 0.1, 0.0, 1.0);
                return;
            }
        }
    }
}