#include "yuy2_to_rgb_shader_program.h"

QnYuy2ToRgbShaderProgram::QnYuy2ToRgbShaderProgram(const QGLContext *context, QObject *parent):
    QnArbShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        !!ARBfp1.0
        PARAM c[5] =    { program.local[0..1],
                        { 0.5, 2, 1, 0.0625 },
                        { 1.164, 0, 1.596, 2.0179999 },
                        { 1.164, -0.391, -0.81300002 } };
        TEMP R0;
        TEMP R1;
        TEMP R2;
        FLR R1.z, fragment.texcoord[0].x;
        ADD R0.x, R1.z, c[2];
        ADD R1.z, fragment.texcoord[0].x, -R1;
        MUL R1.x, fragment.texcoord[0].z, R0;
        MOV R1.y, fragment.texcoord[0];
        TEX R0, R1, texture[0], 2D;
        ADD R1.y, R0.z, -R0.x;
        MUL R2.x, R1.z, R1.y;
        MAD R0.x, R2, c[2].y, R0;
        MOV R1.y, fragment.texcoord[0];
        ADD R1.x, fragment.texcoord[0].z, R1;
        TEX R1.xyw, R1, texture[0], 2D;
        ADD R2.x, R1, -R0.z;
        MAD R1.x, R1.z, c[2].y, -c[2].z;
        MAD R0.z, R1.x, R2.x, R0;
        ADD R1.xy, R1.ywzw, -R0.ywzw;
        ADD R0.z, R0, -R0.x;
        SGE R1.w, R1.z, c[2].x;
        MAD R0.x, R1.w, R0.z, R0;
        MAD R0.yz, R1.z, R1.xxyw, R0.xyww;
        ADD R0.xyz, R0, -c[2].wxxw;
        MUL R0.w, R0.y, c[0];
        MAD R0.w, R0.z, c[0].z, R0;
        MUL R0.z, R0, c[0].w;
        MAD R0.y, R0, c[0].z, R0.z;
        MUL R0.w, R0, c[0].y;
        MUL R0.y, R0, c[0];
        MUL R0.z, R0.w, c[1].x;
        MAD R0.x, R0, c[0].y, c[0];
        MUL R0.y, R0, c[1].x;
        DP3 result.color.x, R0, c[3];
        DP3 result.color.y, R0, c[4];
        DP3 result.color.z, R0, c[3].xwyw;
        MOV result.color.w, c[1].y;
        END
    ));
}

