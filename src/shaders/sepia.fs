#version 330 core

in vec2 fragTexCoord;
out vec4 fragColor;
uniform sampler2D texture0;

void main() {
    vec4 color = texture(texture0, fragTexCoord);
    float r = color.r * 0.393 + color.g * 0.769 + color.b * 0.189;
    float g = color.r * 0.349 + color.g * 0.686 + color.b * 0.168;
    float b = color.r * 0.272 + color.g * 0.534 + color.b * 0.131;
    fragColor = vec4(r, g, b, color.a);
}
