#ifndef QN_TEXTURE_COLOR_SHADER_PROGRAM_H
#define QN_TEXTURE_COLOR_SHADER_PROGRAM_H


#include "color_shader_program.h"

class QnTextureGLShaderProgram : public QnColorGLShaderProgram 
{
    Q_OBJECT
public:
    QnTextureGLShaderProgram(QObject *parent = NULL);

    void setTexture(int target){
        setUniformValue(m_texture, target);
    }
    virtual bool compile();

    virtual bool link() override
    {
        bool rez = QnColorGLShaderProgram::link();
        if (rez) {
            m_texture = uniformLocation("uTexture");
        }
        return rez;
    }
private:
    int m_texture;
};


#endif // QN_TEXTURE_COLOR_SHADER_PROGRAM_H
