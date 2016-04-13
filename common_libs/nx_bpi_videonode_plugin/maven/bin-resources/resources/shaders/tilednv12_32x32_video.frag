uniform sampler2D plane1Texture;
uniform sampler2D plane2Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

uniform int pixelWidth;
uniform int pixelHeight;
uniform int blocksPerLine;

varying highp vec2 plane1TexCoord;

void main()
{
	vec2 intCoord = vec2(floor(plane1TexCoord.x),
	                     floor(plane1TexCoord.y));

	float blockNum = floor(intCoord.y / 32) * blocksPerLine + floor(intCoord.x / 32);
    float offset = blockNum * (32*32) + mod(intCoord.y * 32, 32*32) + mod(intCoord.x, 32);
	vec2 mappedCoord = vec2((mod(offset, pixelWidth) + 0.5) / pixelWidth,
	                        (floor(offset / pixelWidth) + 0.5) / pixelHeight);

	vec2 uvPixelSize = vec2(pixelWidth / 2, pixelHeight / 2);
	float blockNumUV = floor(intCoord.y / 64) * blocksPerLine + floor(intCoord.x / 32);
    float offsetUV = blockNumUV * (16*32) + mod(intCoord.y * 8 , 32*16) + mod(intCoord.x, 16);
	vec2 mappedCoordUV = vec2((mod(offsetUV, uvPixelSize.x) + 0.5) / uvPixelSize.x,
	                        (floor(offsetUV / uvPixelSize.x) + 0.5) / uvPixelSize.y);

	mediump float Y = texture2D(plane1Texture, mappedCoord).r;
    mediump vec2 UV = texture2D(plane2Texture, mappedCoordUV).ra;
    mediump vec4 color = vec4(Y, UV.x, UV.y, 1.);
    gl_FragColor = colorMatrix * color * opacity;
}
