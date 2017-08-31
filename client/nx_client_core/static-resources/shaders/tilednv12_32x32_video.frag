uniform sampler2D plane1Texture;
uniform sampler2D plane2Texture;
uniform mediump mat4 colorMatrix;
uniform lowp float opacity;

uniform mediump int uvBlocksCount;
uniform mediump int yBlocksPerLine;
uniform mediump int yTextureHeight;

varying highp vec2 plane1TexCoord;

void main()
{
    mediump ivec2 columnRow = ivec2(plane1TexCoord) / 32;
    mediump int blockNum = columnRow.y * yBlocksPerLine + columnRow.x;
    mediump ivec2 inBlock = ivec2(mod(plane1TexCoord, 32.0));

    mediump int blockNumUV = columnRow.y / 2 * yBlocksPerLine + columnRow.x;
    mediump ivec2 inBlockUV = ivec2(inBlock.x, int(mod(plane1TexCoord.y, 64.0))) / 2;

    mediump vec2 mappedCoord = vec2(
            mod(float(blockNum), 2.0) / 2.0 + float(inBlock.y) / 64.0 + float(inBlockUV.x) / 1024.0, //  + (0.5 / 1024.0),
            (float(blockNum / 2) + 0.5) / float(uvBlocksCount));

    mediump vec2 mappedCoordUV = vec2(
            (float(inBlockUV.y) * 16.0 + float(inBlockUV.x) + 0.5) / 512.0,
            (float(blockNumUV) + 0.5) / float(uvBlocksCount));

    mediump float Y = texture2D(plane1Texture, mappedCoord).ra[(inBlock - inBlockUV * 2).x];
    mediump vec2 UV = texture2D(plane2Texture, mappedCoordUV).ra;

    mediump vec4 color = vec4(Y, UV.x, UV.y, 1);
    gl_FragColor = colorMatrix * color;
}
