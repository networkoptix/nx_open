uniform sampler2D plane1Texture;
uniform sampler2D plane2Texture;
uniform sampler2D plane3Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

uniform mediump float yBlocksCount;
uniform mediump float yBlocksPerLine;
uniform mediump float yTextureHeight;

varying highp vec2 plane1TexCoord;

void main()
{
    mediump vec2 intCoord = vec2(floor(plane1TexCoord.x), floor(plane1TexCoord.y));

	sampler2D yTexture = (intCoord.y >= yTextureHeight ? plane3Texture : plane1Texture);
	mediump float halfY = mod(intCoord.y, yTextureHeight);
	
	mediump float blockColumn = floor(intCoord.x / 32.0);
	mediump float blockNum = floor(halfY / 32.0) * yBlocksPerLine + blockColumn;
	mediump float xInBlock = mod(intCoord.x, 32.0);
	mediump float yInBlock = mod(halfY, 32.0);
	
	mediump float uvBlocksCount = yBlocksCount / 2.0;
	mediump float blockNumUV = floor(intCoord.y / 64.0) * yBlocksPerLine + blockColumn;
	mediump float xInBlockUV = floor(xInBlock / 2.0);
	mediump float yInBlockUV = floor(mod(intCoord.y, 64.0) / 2.0);

	mediump vec2 mappedCoord = vec2(
			(yInBlock * 32.0 + xInBlock + 0.5) / 1024.0,
			(blockNum + 0.5) / (yBlocksCount/2.0));

	mediump vec2 mappedCoordUV = vec2(
			(yInBlockUV * 16.0 + xInBlockUV + 0.5) / 512.0,
			(blockNumUV + 0.5) / uvBlocksCount);


	mediump float Y = texture2D(yTexture, mappedCoord).r;

	mediump vec2 UV = texture2D(plane2Texture, mappedCoordUV).ra;

	mediump vec4 color = vec4(Y, UV.x, UV.y, 1.0);
	gl_FragColor = colorMatrix * color * opacity;
}
