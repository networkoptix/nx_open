#ifndef QN_TEXTURE_TRANSITION_SHADER_PROGRAM_H
#define QN_TEXTURE_TRANSITION_SHADER_PROGRAM_H

#include <QtOpenGL/QGLShaderProgram>

class QnTextureTransitionShaderProgram: public QGLShaderProgram {
    Q_OBJECT;
public:
    QnTextureTransitionShaderProgram(const QGLContext *context, QObject *parent = NULL);

    void setTexture0(int target) {
        setUniformValue(m_texture0Location, target);
    }

    void setTexture1(int target) {
        setUniformValue(m_texture1Location, target);
    }

    void setProgress(GLfloat progress) {
        setUniformValue(m_progressLocation, progress);
    }

private:
    int m_texture0Location;
    int m_texture1Location;
    int m_progressLocation;
};


#endif //QN_TEXTURE_TRANSITION_SHADER_PROGRAM_H
