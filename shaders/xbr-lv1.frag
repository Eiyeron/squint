#version 330
in vec2 fragTexCoord; // Fragment input attribute: texture coordinate
in vec4 fragColor;    // Fragment input attribute: color
out vec4 finalColor;  // Fragment output: color
uniform sampler2D texture0;
uniform vec4 colDiffuse;

vec4 main_fragment(in sampler2D base_texture,
                   in ivec2 texture_size,
                   in ivec2 base_texel_coord,
                   in vec2 tex_coord);

void main()
{
    vec2 uv = vec2(fragTexCoord.x, fragTexCoord.y);
    ivec2 texSize = textureSize(texture0, 0);
    ivec2 centerCoords = ivec2(uv * vec2(texSize));
    vec4 sampledLvl1 = main_fragment(texture0, texSize, centerCoords, uv);
    finalColor = sampledLvl1;
}

/*
   Hyllian's xBR-lv1-noblend Shader

   Copyright (C) 2011-2014 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.


   -- Adaptation to GLSL + texelFetch by Florian Dormont
*/

uniform int XbrCornerMode = 2;
uniform float XbrYWeight = 48.;
uniform float XbrEqThreshold = 30.;

const mat3 yuv = mat3(0.299, 0.587, 0.114, -0.169, -0.331, 0.499, 0.499, -0.418, -0.0813);

float RGBtoYUV(vec3 color)
{
    return dot(color, XbrYWeight * yuv[0]);
}

float df(float A, float B)
{
    return abs(A - B);
}

bool eq(float A, float B)
{
    return (df(A, B) < XbrEqThreshold);
}

float weighted_distance(float a, float b, float c, float d, float e, float f, float g, float h)
{
    return (df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h));
}

/*
    xBR LVL1 works over the pixels below:

        |B |C |
     |D |E |F |F4|
     |G |H |I |I4|
        |H5|I5|

    Consider E as the central pixel. xBR LVL1 needs only to look at 12 texture pixels.
*/

// Purposedly returns a pixel with a magenta color so out of bounds to use it as
// an alpha mask.
// [TODO] Maybe use instead an external boolean flag?
vec3 fetchOrMagenta(in sampler2D tex, in ivec2 texel_coords)
{
    vec4 texel = texelFetch(tex, texel_coords, 0);
    if ((texel.a) == 0)
    {
        return vec3(1000,1000,1000);
    }
    return texel.rgb;
}

/*    FRAGMENT SHADER    */
vec4 main_fragment(in sampler2D base_texture,
                   in ivec2 texture_size,
                   in ivec2 base_texel_coord,
                   in vec2 tex_coord)
{
    bool edr, px; // px = pixel, edr = edge detection rule
    bool interp_restriction_lv1;
    bool nc; // new_color
    bool fx; // inequations of straight lines.

    vec2 pos = mod(tex_coord * texture_size, 1.) - vec2(0.5, 0.5); // pos = pixel position
    ivec2 dir = ivec2(sign(pos));                                  // dir = pixel direction

    ivec2 g1 = dir * ivec2(0, -1);
    ivec2 g2 = dir * ivec2(-1, 0);

    vec3 B = fetchOrMagenta(base_texture, base_texel_coord + g1);
    vec3 C = fetchOrMagenta(base_texture, base_texel_coord + g1 - g2);
    vec3 D = fetchOrMagenta(base_texture, base_texel_coord + g2);
    vec3 E = fetchOrMagenta(base_texture, base_texel_coord);
    vec3 F = fetchOrMagenta(base_texture, base_texel_coord - g2);
    vec3 G = fetchOrMagenta(base_texture, base_texel_coord - g1 + g2);
    vec3 H = fetchOrMagenta(base_texture, base_texel_coord - g1);
    vec3 I = fetchOrMagenta(base_texture, base_texel_coord - g1 - g2);

    vec3 F4 = fetchOrMagenta(base_texture, base_texel_coord - 2 * g2);
    vec3 I4 = fetchOrMagenta(base_texture, base_texel_coord - g1 - 2 * g2);
    vec3 H5 = fetchOrMagenta(base_texture, base_texel_coord - 2 * g1);
    vec3 I5 = fetchOrMagenta(base_texture, base_texel_coord - 2 * g1 - g2);

    float b = RGBtoYUV(B);
    float c = RGBtoYUV(C);
    float d = RGBtoYUV(D);
    float e = RGBtoYUV(E);
    float f = RGBtoYUV(F);
    float g = RGBtoYUV(G);
    float h = RGBtoYUV(H);
    float i = RGBtoYUV(I);

    float i4 = RGBtoYUV(I4);
    float i5 = RGBtoYUV(I5);
    float h5 = RGBtoYUV(H5);
    float f4 = RGBtoYUV(F4);

    fx = (dot(dir, pos) > 0.5);

    if (XbrCornerMode == 0)
    {
        interp_restriction_lv1 = ((e != f) && (e != h));

    }
    else if (XbrCornerMode == 1)
    {
        interp_restriction_lv1 = ((e != f) && (e != h) &&
                                (!eq(f, b) && !eq(h, d) || eq(e, i) && !eq(f, i4) && !eq(h, i5) ||
                                eq(e, g) || eq(e, c)));
    }
    else
    {
        interp_restriction_lv1 =
            ((e != f) && (e != h) &&
            (!eq(f, b) && !eq(f, c) || !eq(h, d) && !eq(h, g) ||
            eq(e, i) && (!eq(f, f4) && !eq(f, i4) || !eq(h, h5) && !eq(h, i5)) || eq(e, g) ||
            eq(e, c)));
    }

    edr = (weighted_distance(e, c, g, i, h5, f4, h, f) <
           weighted_distance(h, d, i5, f, i4, b, e, i)) &&
          interp_restriction_lv1;

    nc = (edr && fx);

    px = (df(e, f) <= df(e, h));

    vec3 res = nc ? px ? F : H : E;

    if (res.x > 1)
        discard;
    return vec4(res, 1.0);
}