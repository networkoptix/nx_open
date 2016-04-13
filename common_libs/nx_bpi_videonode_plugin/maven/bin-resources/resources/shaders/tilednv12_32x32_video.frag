uniform sampler2D plane1Texture;
uniform sampler2D plane2Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

uniform mediump float pixelWidth;
uniform mediump float pixelHeight;
uniform mediump float blocksPerLine;

varying highp vec2 plane1TexCoord;

void main()
{
    mediump vec2 intCoord = vec2(floor(plane1TexCoord.x),
                                 floor(plane1TexCoord.y));

    mediump float blockNum = floor(intCoord.y / 32.0) * blocksPerLine + floor(intCoord.x / 32.0);
    mediump float offset = blockNum * (32.0 * 32.0) + mod(intCoord.y * 32.0, 32.0*32.0) + mod(intCoord.x, 32.0);
    mediump vec2 mappedCoord = vec2((mod(offset, pixelWidth) + 0.5) / pixelWidth,
                            (floor(offset / pixelWidth) + 0.5) / pixelHeight);

    mediump vec2 uvPixelSize = vec2(pixelWidth / 2.0, pixelHeight / 2.0);
    mediump float blockNumUV = floor(intCoord.y / 64.0) * blocksPerLine + floor(intCoord.x / 32.0);
    mediump float offsetUV = blockNumUV * (16.0 * 32.0) + mod(intCoord.y * 8.0 , 32.0 * 16.0) + mod(intCoord.x, 16.0);
    mediump vec2 mappedCoordUV = vec2((mod(offsetUV, uvPixelSize.x) + 0.5) / uvPixelSize.x,
	                        (floor(offsetUV / uvPixelSize.x) + 0.5) / uvPixelSize.y);

    mediump float Y = texture2D(plane1Texture, mappedCoord).r;
    mediump vec2 UV = texture2D(plane2Texture, mappedCoordUV).ra;
    mediump vec4 color = vec4(Y, UV.x, UV.y, 1.0);
    gl_FragColor = colorMatrix * color * opacity;
}
