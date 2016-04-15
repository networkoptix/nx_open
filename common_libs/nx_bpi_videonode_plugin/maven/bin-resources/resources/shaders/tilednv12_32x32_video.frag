uniform sampler2D plane1Textures[2];
uniform sampler2D plane2Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

uniform mediump float yBlocksCount;
uniform mediump float yBlocksPerLine;
uniform mediump float yTextureHeight;

varying highp vec2 plane1TexCoord;

void main()
{
    mediump float intX = floor(plane1TexCoord.x);
	mediump float halfY = floor(mod(plane1TexCoord.y, yTextureHeight));

	mediump float blockColumn = floor(intX / 32.0);
	mediump float blockNum = floor(halfY / 32.0) * yBlocksPerLine + blockColumn;
	mediump float xInBlock = mod(intX, 32.0);
	mediump float yInBlock = mod(halfY, 32.0);

	mediump float uvBlocksCount = yBlocksCount / 2.0;
	mediump float blockNumUV = floor(plane1TexCoord.y / 64.0) * yBlocksPerLine + blockColumn;
	mediump float xInBlockUV = floor(xInBlock / 2.0);
	mediump float yInBlockUV = floor(mod(plane1TexCoord.y, 64.0) / 2.0);

	mediump vec2 mappedCoord = vec2(
			(yInBlock * 32.0 + xInBlock + 0.5) / 1024.0,
			(blockNum + 0.5) / (yBlocksCount / 2.0));

	mediump vec2 mappedCoordUV = vec2(
			(yInBlockUV * 16.0 + xInBlockUV + 0.5) / 512.0,
			(blockNumUV + 0.5) / uvBlocksCount);


	mediump float Y = texture2D(plane1Textures[int(plane1TexCoord.y / yTextureHeight)], mappedCoord).r;

	mediump vec2 UV = texture2D(plane2Texture, mappedCoordUV).ra;

	mediump vec4 color = vec4(Y, UV.x, UV.y, 1.0);
	gl_FragColor = colorMatrix * color * opacity;
}
