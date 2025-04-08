#version 330 core

in vec2 fragTexCoord;
out vec4 fragColor;
uniform sampler2D texture0;
uniform float time;

void main() {
    vec2 uv = fragTexCoord;
    uv.x += sin(uv.y * 15.0 + time * 2.0) * 0.02;
    uv.y += cos(uv.x * 15.0 + time * 2.0) * 0.02;
    vec4 color = texture(texture0, uv);
    fragColor = vec4(color.r * 0.8, color.g * 0.9, color.b * 1.1, color.a);
}
