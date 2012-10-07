#ifndef QN_COLOR_SHADER_PROGRAM_H
#define QN_COLOR_SHADER_PROGRAM_H

#include <QtCore/QSharedPointer>
#include <QtOpenGL/QGLShaderProgram>

class QnColorShaderProgram: public QGLShaderProgram {
    Q_OBJECT;
public:
    QnColorShaderProgram(const QGLContext *context, QObject *parent = NULL);

    /**
     * \param context                   OpenGL context to get an instance of this shader for.
     * \returns                         Shared instance of this shader that can be used with the given OpenGL context.
     */
    static QSharedPointer<QnColorShaderProgram> instance(const QGLContext *context);

    template<class T>
    void setColorMultiplier(const T &value) {
        setUniformValue(m_colorMultiplierLocation, value);
    }

    template<class T>
    void setColor(const T &value) {
        setAttributeValue(m_colorLocation, value);
    }

    void setColorBuffer(GLenum type, int offset, int tupleSize, int stride = 0) {
        setAttributeBuffer(m_colorLocation, type, offset, tupleSize, stride);
    }

private:
    int m_colorMultiplierLocation, m_colorLocation;
};

#endif // QN_COLOR_SHADER_PROGRAM_H
