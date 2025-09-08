vec4 rvo_hash4(vec2 p) { return fract(sin(vec4(1.0 + dot(p, vec2(37.0, 17.0)), 
                                               2.0 + dot(p, vec2(11.0, 47.0)),
                                               3.0 + dot(p, vec2(41.0, 29.0)),
                                               4.0 + dot(p, vec2(23.0, 31.0)))) * 103.0); }

vec4 rvo_texture_no_tile(sampler2D samp, in vec2 uv) {
    ivec2 iuv = ivec2(floor(uv));
    vec2 fuv = fract(uv);

    vec4 ofa = rvo_hash4(iuv + ivec2(0, 0));
    vec4 ofb = rvo_hash4(iuv + ivec2(1, 0));
    vec4 ofc = rvo_hash4(iuv + ivec2(0, 1));
    vec4 ofd = rvo_hash4(iuv + ivec2(1, 1));
    
    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);

    ofa.zw = sign(ofa.zw - 0.5);
    ofb.zw = sign(ofb.zw - 0.5);
    ofc.zw = sign(ofc.zw - 0.5);
    ofd.zw = sign(ofd.zw - 0.5);
    
    vec2 uva = uv * ofa.zw + ofa.xy, ddxa = ddx * ofa.zw, ddya = ddy * ofa.zw;
    vec2 uvb = uv * ofb.zw + ofb.xy, ddxb = ddx * ofb.zw, ddyb = ddy * ofb.zw;
    vec2 uvc = uv * ofc.zw + ofc.xy, ddxc = ddx * ofc.zw, ddyc = ddy * ofc.zw;
    vec2 uvd = uv * ofd.zw + ofd.xy, ddxd = ddx * ofd.zw, ddyd = ddy * ofd.zw;
   	 
    vec2 b = smoothstep(0.25, 0.75, fuv);
    
    return mix(mix(textureGrad(samp, uva, ddxa, ddya),
                   textureGrad(samp, uvb, ddxb, ddyb), b.x),
               mix(textureGrad(samp, uvc, ddxc, ddyc),
                   textureGrad(samp, uvd, ddxd, ddyd), b.x), b.y);
}