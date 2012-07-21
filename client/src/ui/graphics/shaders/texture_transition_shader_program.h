#ifndef QN_TEXTURE_TRANSITION_SHADER_PROGRAM_H
#define QN_TEXTURE_TRANSITION_SHADER_PROGRAM_H

#include <QGLShaderProgram>

class QnTextureTransitionShaderProgram: public QGLShaderProgram {
    Q_OBJECT;
public:
    QnTextureTransitionShaderProgram(const QGLContext *context, QObject *parent = NULL);

    void setTexture0(int target);
    void setTexture1(int target);
    void setProgress(qreal progress);

private:
    int m_texture0Location;
    int m_texture1Location;
    int m_progressLocation;
};


#endif QN_TEXTURE_TRANSITION_SHADER_PROGRAM_H
