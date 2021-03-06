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
    vec2 uv = fragTexCoord;
    ivec2 texSize = textureSize(texture0, 0);
    ivec2 centerCoords = ivec2(uv * vec2(texSize));
    vec4 sampledLvl1 = main_fragment(texture0, texSize, centerCoords, uv);
    finalColor = sampledLvl1;
}

/*
   Hyllian's xBR-lv2 Shader

   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com

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

   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.

   -- Adaptation to GLSL + texelFetch by Florian Dormont
*/

uniform int XbrCornerMode = 0;
uniform int XbrScale = 4;
uniform float XbrYWeight = 48.;
uniform float XbrEqThreshold = 25.;
uniform float XbrLv2Coefficient = 2.;

const vec4 Ao = vec4(1.0, -1.0, -1.0, 1.0);
const vec4 Bo = vec4(1.0, 1.0, -1.0, -1.0);
const vec4 Co = vec4(1.5, 0.5, -0.5, 0.5);
const vec4 Ax = vec4(1.0, -1.0, -1.0, 1.0);
const vec4 Bx = vec4(0.5, 2.0, -0.5, -2.0);
const vec4 Cx = vec4(1.0, 1.0, -0.5, 0.0);
const vec4 Ay = vec4(1.0, -1.0, -1.0, 1.0);
const vec4 By = vec4(2.0, 0.5, -2.0, -0.5);
const vec4 Cy = vec4(2.0, 0.0, -1.0, 0.5);
const vec4 Ci = vec4(0.25, 0.25, 0.25, 0.25);

const vec3 Y = vec3(0.2126, 0.7152, 0.0722);

vec4 df(vec4 A, vec4 B)
{
    return abs(A - B);
}

float c_df(vec3 c1, vec3 c2)
{
    vec3 df = abs(c1 - c2);
    return df.r + df.g + df.b;
}

bvec4 eq(vec4 A, vec4 B)
{
    return lessThan(df(A, B), vec4(XbrEqThreshold));
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
    return (df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h));
}

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

// [HACK] Replacment of b/vec4 logic operators
// as they're broken in some Intel HD driver implementations.
bvec4 comp_neg(in bvec4 v)
{
    return bvec4(!v.x, !v.y, !v.z, !v.w);
}

bvec4 comp_and(in bvec4 a, in bvec4 b)
{
    return bvec4(a.x && b.x, a.y && b.y, a.z && b.z, a.w && b.w);
}

bvec4 comp_or(in bvec4 a, in bvec4 b)
{
    return bvec4(a.x || b.x, a.y || b.y, a.z || b.z, a.w || b.w);
}

// Kept for refernce : the neighbor fragment naming.
//    A1 B1 C1
// A0  A  B  C C4
// D0  D  E  F F4
// G0  G  H  I I4
//    G5 H5 I5

/*    FRAGMENT SHADER    */
vec4 main_fragment(in sampler2D base_texture,
                   in ivec2 base_texture_size,
                   in ivec2 base_texel_coord,
                   in vec2 tex_coord)
{
    bvec4 edri, edr, edr_left, edr_up, px; // px = pixel, edr = edge detection rule
    bvec4 interp_restriction_lv0, interp_restriction_lv1, interp_restriction_lv2_left,
        interp_restriction_lv2_up;
    vec4 fx, fx_left, fx_up; // inequations of straight lines.

    vec4 delta = vec4(1.0 / float(XbrScale), 1.0 / float(XbrScale), 1.0 / float(XbrScale), 1.0 / float(XbrScale));
    vec4 deltaL = vec4(0.5 / float(XbrScale), 1.0 / float(XbrScale), 0.5 / float(XbrScale), 1.0 / float(XbrScale));
    vec4 deltaU = deltaL.yxwz;

    vec2 fp = fract(base_texture_size * tex_coord);

    vec3 A1 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-1, -2));
    vec3 B1 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(0, -2));
    vec3 C1 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(1, -2));

    vec3 A = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-1, -1));
    vec3 B = fetchOrMagenta(base_texture, base_texel_coord + ivec2(0, -1));
    vec3 C = fetchOrMagenta(base_texture, base_texel_coord + ivec2(1, -1));

    vec3 D = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-1, 0));
    vec3 E = fetchOrMagenta(base_texture, base_texel_coord + ivec2(0, 0));
    vec3 F = fetchOrMagenta(base_texture, base_texel_coord + ivec2(1, 0));

    vec3 G = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-1, 1));
    vec3 H = fetchOrMagenta(base_texture, base_texel_coord + ivec2(0, 1));
    vec3 I = fetchOrMagenta(base_texture, base_texel_coord + ivec2(1, 1));

    vec3 G5 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-1, 2));
    vec3 H5 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(0, 2));
    vec3 I5 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(1, 2));

    vec3 A0 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-2, -1));
    vec3 D0 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-2, 0));
    vec3 G0 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(-2, 1));

    vec3 C4 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(2, -1));
    vec3 F4 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(2, 0));
    vec3 I4 = fetchOrMagenta(base_texture, base_texel_coord + ivec2(2, 1));

    vec4 b = (XbrYWeight * Y) * mat4x3(B, D, H, F);
    vec4 c = (XbrYWeight * Y) * mat4x3(C, A, G, I);
    vec4 e = (XbrYWeight * Y) * mat4x3(E, E, E, E);
    vec4 d = b.yzwx;
    vec4 f = b.wxyz;
    vec4 g = c.zwxy;
    vec4 h = b.zwxy;
    vec4 i = c.wxyz;

    vec4 i4 = (XbrYWeight * Y) * mat4x3(I4, C1, A0, G5);
    vec4 i5 = (XbrYWeight * Y) * mat4x3(I5, C4, A1, G0);
    vec4 h5 = (XbrYWeight * Y) * mat4x3(H5, F4, B1, D0);
    vec4 f4 = h5.yzwx;

    // These inequations define the line below which interpolation occurs.
    fx = (Ao * fp.y + (Bo * fp.x));
    fx_left = (Ax * fp.y + (Bx * fp.x));
    fx_up = (Ay * fp.y + (By * fp.x));

    interp_restriction_lv0 = comp_and(notEqual(e, f), notEqual(e, h));
    interp_restriction_lv1 = interp_restriction_lv0;

    if (XbrCornerMode == 1)
    {
// Kept for reference
//        interp_restriction_lv1 = (interp_restriction_lv0 &&
//                                  (!eq(f, b) && !eq(h, d) || eq(e, i) && !eq(f, i4) && !eq(h, i5) ||
//                                   eq(e, g) || eq(e, c)));

        bvec4 interp_rule_1 = comp_and(comp_neg(eq(f, b)), comp_neg(eq(h, d)));
        bvec4 interp_rule_2 = comp_and(
                                comp_and(
                                    eq(e, i),
                                    comp_neg(eq(f, i4))),
                                comp_neg(eq(h, i5)));
        bvec4 interp_rule_3 = eq(e, g);
        bvec4 interp_rule_4 = eq(e, c);
        bvec4 interp_rule = comp_or(interp_rule_1, interp_rule_2);
        interp_rule = comp_or(interp_rule, interp_rule_3);
        interp_rule = comp_or(interp_rule, interp_rule_4);

        interp_restriction_lv1 = comp_and(interp_restriction_lv0, interp_rule);
    }

    if (XbrCornerMode == 3)
    {
// Kept for reference
//        interp_restriction_lv1 = (interp_restriction_lv0 &&
//                                  (!eq(f, b) && !eq(h, d) || eq(e, i) && !eq(f, i4) && !eq(h, i5) ||
//                                   eq(e, g) || eq(e, c)) &&
//                                  (f != f4 && f != i || h != h5 && h != i || h != g || f != c ||
//                                   eq(b, c1) && eq(d, g0)));
//
        vec4 c1 = i4.yzwx;
        vec4 g0 = i5.wxyz;
        bvec4 interp_rule_1 = comp_and(comp_neg(eq(f, b)), comp_neg(eq(h, d)));
        bvec4 interp_rule_2 = comp_and(
                                comp_and(
                                    eq(e, i),
                                    comp_neg(eq(f, i4))),
                                comp_neg(eq(h, i5)));
        bvec4 interp_rule_3 = eq(e, g);
        bvec4 interp_rule_4 = eq(e, c);
        {

            bool match_1 = f != f4 && f != i;
            bool match_2 = h != h5 && h != i;
            bool match_3 = h != g;
            bool match_4 = f != c;
            bvec4 match_5 = comp_and(eq(b, c1), eq(d, g0));

            bvec4 interm = comp_or(bvec4(match_1), bvec4(match_2));
            interm = comp_or(interm, bvec4(match_3));
            interm = comp_or(interm, bvec4(match_4));
            interp_rule_4 = comp_and(interp_rule_4, interm);
        }
    }

   if (XbrCornerMode == 2)
    {
// Kept for reference
//        interp_restriction_lv1 =
//            (interp_restriction_lv0 &&
//             (!eq(f, b) && !eq(f, c) || !eq(h, d) && !eq(h, g) ||
//              eq(e, i) && (!eq(f, f4) && !eq(f, i4) || !eq(h, h5) && !eq(h, i5)) || eq(e, g) ||
//              eq(e, c)));

        bvec4 match_1 = comp_and(comp_neg(eq(f, b)), comp_neg(eq(f, c)));
        bvec4 match_2 = comp_and(comp_neg(eq(h, d)), comp_neg(eq(h, g)));

            bvec4 match_3a = eq(e, i);
            bvec4 match_3b = comp_and(comp_neg(eq(f, f4)), comp_neg(eq(f, i4)));
        bvec4 match_3 = comp_and(match_3a, match_3b);

        bvec4 match_4 = comp_and(comp_neg(eq(h, h5)), comp_neg(eq(h, i5)));
        bvec4 match_5 = eq(e, g);
        bvec4 match_6 = eq(e, c);


        interp_restriction_lv1 = comp_or(match_1, match_2);
        interp_restriction_lv1 = comp_or(interp_restriction_lv1, match_3);
        interp_restriction_lv1 = comp_or(interp_restriction_lv1, match_4);
        interp_restriction_lv1 = comp_or(interp_restriction_lv1, match_5);
        interp_restriction_lv1 = comp_or(interp_restriction_lv1, match_6);
        interp_restriction_lv1 = comp_and(interp_restriction_lv0, interp_restriction_lv1);
    }

    interp_restriction_lv2_left = comp_and(notEqual(e, g), notEqual(d, g));
    interp_restriction_lv2_up = comp_and(notEqual(e, c), notEqual(b, c));

    vec4 wd1 = weighted_distance(e, c, g, i, h5, f4, h, f);

    vec4 wd2 = weighted_distance(h, d, i5, f, i4, b, e, i);

    edri = comp_and(lessThanEqual(wd1, wd2), interp_restriction_lv0);
    edr = comp_and(lessThan(wd1, wd2), interp_restriction_lv1);

    if (XbrCornerMode == 0)
    {
// Kept for reference
//         edr = edr && (!edri.yzwx || !edri.wxyz);
//         edr_left = lessThanEqual((XbrLv2Coefficient * df(f, g)), df(h, c)) &&
//                    interp_restriction_lv2_left && edr && (!edri.yzwx && eq(e, c));
//         edr_up = greaterThanEqual(df(f, g), (XbrLv2Coefficient * df(h, c))) &&
//                  interp_restriction_lv2_up && edr && (!edri.wxyz && eq(e, g));

        edr = comp_and(edr, comp_or(comp_neg(edri.yzwx), comp_neg(edri.wxyz)));

        edr_left = lessThanEqual(XbrLv2Coefficient * df(f, g), df(h, c));
        edr_left = comp_and(edr_left, interp_restriction_lv2_left);
        edr_left = comp_and(edr_left, edr);
        // [BUG] On Intel iGPU, inlining this variable into the next instruction breaks the upscaler
        bvec4 erd_left_inter = comp_and(comp_neg(edri.yzwx), eq(e, c));
        edr_left = comp_and(edr_left, erd_left_inter);

        edr_up = greaterThanEqual(df(f, g), XbrLv2Coefficient * df(h, c));
        edr_up = comp_and(edr_up, interp_restriction_lv2_up);
        edr_up = comp_and(edr_up, edr);
        // [BUG] On Intel iGPU, inlining this variable into the next instruction breaks the upscaler
        bvec4 erd_up_inter = comp_and(comp_neg(edri.wxyz), eq(e, g));
        edr_up = comp_and(edr_up, erd_up_inter);
    }
    else
    {
// Kept for reference
//        edr_left =
//            ((XbrLv2Coefficient * df(f, g)) <= df(h, c)) && interp_restriction_lv2_left && edr;
//        edr_up = (df(f, g) >= (XbrLv2Coefficient * df(h, c))) && interp_restriction_lv2_up && edr;

        edr_left = lessThanEqual(XbrLv2Coefficient * df(f, g), df(h, c));
        edr_left = comp_and(edr_left, interp_restriction_lv2_left);
        edr_left = comp_and(edr_left, edr);

        edr_up = greaterThanEqual(df(f, g), XbrLv2Coefficient * df(h, c));
        edr_up = comp_and(edr_up, interp_restriction_lv2_up);
        edr_up = comp_and(edr_up, edr);

    }
    vec4 fx45i = clamp((fx + delta - Co - Ci) / (2. * delta), 0., 1.);
    vec4 fx45 = clamp((fx + delta - Co) / (2. * delta), 0., 1.);
    vec4 fx30 = clamp((fx_left + deltaL - Cx) / (2. * deltaL), 0., 1.);
    vec4 fx60 = clamp((fx_up + deltaU - Cy) / (2. * deltaU), 0., 1.);

    fx45 = vec4(edr) * fx45;
    fx30 = vec4(edr_left) * fx30;
    fx60 = vec4(edr_up) * fx60;
    fx45i = vec4(edri) * fx45i;


    px = lessThanEqual(df(e, f), df(e, h));

    vec4 maximos = max(max(fx30, fx60), max(fx45, fx45i));

    vec3 res1 = E;
    res1 = mix(res1, mix(H, F, float(px.x)), maximos.x);
    res1 = mix(res1, mix(B, D, float(px.z)), maximos.z);

    vec3 res2 = E;
    res2 = mix(res2, mix(F, B, float(px.y)), maximos.y);
    res2 = mix(res2, mix(D, H, float(px.w)), maximos.w);

    vec3 res = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));

    if (res.x > 1)
        discard;
    return vec4(res, 1.0);
}
