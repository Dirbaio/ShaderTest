
uniform float time;

float dist(vec3 p)
{
    vec3 p2 = fract(p);
    p = floor(p);
    return length(p2-vec3(0.5, 0.5, 0.5)) - 0.2 + sin(time*2.0 + p.x*3.0 + p.y*0.9+p.z*0.8)*0.15;
}


void main()
{

    float time2 = time*.3;
    //Camera animation
    vec3 vuv=vec3(sin(time2),1.0,0.0);//view up vector
//    vec3 vrp=vec3(sin(time*0.7)*1.0,0,cos(time*0.9)*1.0); //view reference point
    vec3 vrp=vec3(0, 0, 0); //view reference point
    vec3 prp=vec3(sin(time2*0.7)*2.0+vrp.x+2.0,
                  sin(time2)*1.0+4.0+vrp.y+0.0,
                  cos(time2*0.6)*2.0+vrp.z+1.0); //camera position

    //Camera setup
    vec3 vpn=normalize(vrp-prp);
    vec3 u=normalize(cross(vuv,vpn));
    vec3 v=cross(vpn,u);
    vec3 vcv=(prp+vpn);
    vec3 scrCoord=vcv + gl_TexCoord[0].x*u + gl_TexCoord[0].y*v;
    vec3 scp=normalize(scrCoord-prp);


    vec3 pos = prp; //vec3(0, 3, -7.0);
    vec3 dir = scp; //vec3(gl_TexCoord[0].xy, 1.0);

    vec3 color = vec3(0.0, 0.0, 0.0);
    float reflexionIndex = 0.4;
    float colorApport = 0.7;

    vec3 lightDir = vec3(sin(time), cos(time), sin(2*time)-0.3);
    lightDir = normalize(lightDir);
    float w = 0.1;
    for(int i = 0; i < 60; i++)
    {
        if(abs(w)<0.0001) {
            float s = 0.01;
            float d = dist(pos);
            vec3 normal = vec3(
                    d - dist(pos+vec3(s, 0.0, 0.0)),
                    d - dist(pos+vec3(0.0, s, 0.0)),
                    d - dist(pos+vec3(0.0, 0.0, s))
                );
            normal *= (1.0/s);
            normal = normalize(normal);

            dir = reflect(dir, normal);

            // Color
            vec3 col = vec3(sin(pos.x*0.3+time), sin(pos.y*0.5+time*2.0), cos(pos.z*0.4));
            col = (col+vec3(1.0, 1.0, 1.0))/2.0;

            //Diffuse and specular light
            float diffuse = max(dot(normal, lightDir),0.1) * 0.7;
            float specPow = 20.0;
            float specAmt = 0.5;
            float specular = clamp (specAmt * pow(dot(-dir, lightDir), 0.3*specPow) , 0.0, 1.0 );

            color += col*(diffuse+specular)*colorApport;
            colorApport = colorApport*reflexionIndex;
            //if (colorApport < 0.1) break;

            pos = pos + dir;
            w = 0.1;
        }


        w = dist(pos);
        pos += dir*w;
    }
//if (abs(w)>0.03) discard;

    gl_FragColor = vec4(color, time);
}
