#ifndef QN_COLOR_SHADER_PROGRAM_H
#define QN_COLOR_SHADER_PROGRAM_H

#include <QGLShaderProgram>

class QnColorShaderProgram: public QGLShaderProgram {
    Q_OBJECT;
public:
    QnColorShaderProgram(const QGLContext *context, QObject *parent = NULL);

    template<class T>
    void setColorMultiplier(const T &value) {
        setUniformValue(m_colorMultiplierLocation, value);
    }

private:
    int m_colorMultiplierLocation;
};

#endif // QN_COLOR_SHADER_PROGRAM_H
