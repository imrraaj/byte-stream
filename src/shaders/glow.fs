#version 330 core

in vec2 fragTexCoord;
out vec4 fragColor;
uniform sampler2D texture0;
uniform float time; // Add this uniform for animation

void main() {
    vec4 color = texture(texture0, fragTexCoord);
    vec2 offset = vec2(sin(time + fragTexCoord.y * 10.0), cos(time + fragTexCoord.x * 10.0)) * 0.005;
    vec4 glow = texture(texture0, fragTexCoord + offset);
    float intensity = 1.5 + sin(time) * 0.5;
    fragColor = mix(color, glow, 0.3) * intensity;
    fragColor.a = color.a;
}
