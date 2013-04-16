
uniform sampler2D fft;

void main(void)
{
    float x = gl_TexCoord[0].x*0.5+0.5;
    x = exp(x)*0.06-0.06;
    float y = gl_TexCoord[0].y*0.5+0.5;
    float val = texture2D(fft, vec2(x, 0)).x;
    if(val < y)
        val = 0.0;
    else
        val = 1.0;

    gl_FragColor = vec4(1, 1, 1, val);
}
