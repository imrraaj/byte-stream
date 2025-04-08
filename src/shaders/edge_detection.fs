#version 330 core

in vec2 fragTexCoord;
out vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 texSize; // Texture resolution

void main() {
    vec2 offset = 1.0 / texSize;
    vec4 center = texture(texture0, fragTexCoord);
    vec4 left = texture(texture0, fragTexCoord + vec2(-offset.x, 0));
    vec4 right = texture(texture0, fragTexCoord + vec2(offset.x, 0));
    vec4 up = texture(texture0, fragTexCoord + vec2(0, -offset.y));
    vec4 down = texture(texture0, fragTexCoord + vec2(0, offset.y));
    
    vec4 edge = 4.0 * center - left - right - up - down;
    float edgeVal = length(edge.rgb);
    fragColor = vec4(vec3(edgeVal), center.a);
}
