
precision highp float;

void main()
{
    const vec2 coords[] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    gl_Position = vec4(coords[gl_VertexID], 0.0, 1.0);
}
