// Image display

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution;

    gl_FragColor = texture2D(iFrame, uv);

    vec4 i = texture2D(iImage, uv);
    i.a *= iIntensity;

    gl_FragColor = composite(gl_FragColor, i);
}
