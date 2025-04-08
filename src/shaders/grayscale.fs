#version 330 core

in vec2 fragTexCoord; // Texture coordinates
out vec4 fragColor; // Output color

uniform sampler2D texture0; // Texture sampler

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 gray = vec3(1 * texelColor.r + 0 * texelColor.g + 0 * texelColor.b);
	// fragColor = vec4(gray, texelColor.a);
    fragColor = vec4(0.5 * texelColor.r, 0.5 * texelColor.g, 0 , 1.0);
}
