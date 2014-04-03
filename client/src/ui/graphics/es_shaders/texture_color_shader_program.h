#ifndef QN_TEXTURE_COLOR_SHADER_PROGRAM_H
#define QN_TEXTURE_COLOR_SHADER_PROGRAM_H

#ifdef QT_OPENGL_ES_2

#include "color_shader_program.h"

class QnTextureColorGLShaderProgramm : public QnColorGLShaderProgramm 
{
    Q_OBJECT
public:
    QnTextureColorGLShaderProgramm(const QGLContext *context = NULL, QObject *parent = NULL);

    void setTexture(int target){
        setUniformValue(m_texture, target);
    }
    virtual bool compile();

    virtual bool link() override
    {
        bool rez = QnColorGLShaderProgramm::link();
        if (rez) {
            m_texture = uniformLocation("uTexture");
        }
        return rez;
    }
private:
    int m_texture;
};

#endif //QT_OPENGL_ES_2
#endif // QN_TEXTURE_COLOR_SHADER_PROGRAM_H