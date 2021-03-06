// Pixels radiating from the center

void main(void) {
    vec2 uv = gl_FragCoord.xy / iResolution;

    gl_FragColor = texture2D(iFrame, uv);
    vec4 c = texture2D(iChannel[1], uv);
    c.a *= smoothstep(0., 0.2, iIntensity);
    gl_FragColor = composite(gl_FragColor, c);
}
