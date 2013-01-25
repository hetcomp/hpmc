layout(points, invocations=1) in;
layout(triangle_strip, max_vertices=3) out;

uniform mat4            modelviewprojection;
uniform mat3            normalmatrix;

in VG {
    uvec3 i0;
    uint mc_case;
    uint remainder;
} in_g[];


out GO {
    vec3                normal;
} out_g;


void
mc_extract( out vec3 P, out vec3 N, in uvec3 i0, in uint mc_case, in uint remainder );

void
main()
{
    for(int i=0; i<3; i++ ) {
        vec3 P;
        vec3 N;
        mc_extract( P, N, in_g[0].i0, in_g[0].mc_case, in_g[0].remainder + i );
        out_g.normal = normalmatrix * N;
        gl_Position = modelviewprojection * vec4( P, 1.0 );
        EmitVertex();
    }
}
