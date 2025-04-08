#version 330 core

in vec2 fragTexCoord;
out vec4 fragColor;
uniform sampler2D texture0;
uniform float pixelSize = 128.0; // Adjust this for pixel size

void main() {
    vec2 uv = floor(fragTexCoord * pixelSize) / pixelSize;
    fragColor = texture(texture0, uv);
}
