#ifndef QN_COLOR_SHADER_PROGRAM_H
#define QN_COLOR_SHADER_PROGRAM_H

#ifdef QT_OPENGL_ES_2

#include <QtCore/QSharedPointer>
#include <QtOpenGL/QGLShaderProgram>
#include "base_shader_program.h"
//
//class QnColorShaderProgram: public QGLShaderProgram {
//    Q_OBJECT
//public:
//    QnColorShaderProgram(const QGLContext *context, QObject *parent = NULL);
//
//    /**
//     * \param context                   OpenGL context to get an instance of this shader for.
//     * \returns                         Shared instance of this shader that can be used with the given OpenGL context.
//     */
//    static QSharedPointer<QnColorShaderProgram> instance(const QGLContext *context);
//
//    template<class T>
//    void setColorMultiplier(const T &value) {
//        setUniformValue(m_colorMultiplierLocation, value);
//    }
//
//    template<class T>
//    void setColor(const T &value) {
//        setAttributeValue(m_colorLocation, value);
//    }
//
//    int colorMultiplierLocation() const {
//        return m_colorMultiplierLocation;
//    }
//
//    int colorLocation() const {
//        return m_colorLocation;
//    }
//
//private:
//    int m_colorMultiplierLocation, m_colorLocation;
//};

class QnColorGLShaderProgramm : public QnAbstractBaseGLShaderProgramm 
{
    Q_OBJECT
public:
    QnColorGLShaderProgramm(const QGLContext *context = NULL, QObject *parent = NULL);

    void setColor(const QVector4D& a_vec){
        setUniformValue(m_color, a_vec);
    }

    virtual bool compile();

    virtual bool link() override
    {
        bool rez = QnAbstractBaseGLShaderProgramm::link();
        if (rez) {
            m_color = uniformLocation("uColor");
        }
        return rez;
    }
private:
    int m_color;
};

#endif // QT_OPENGL_ES_2

#endif // QN_COLOR_SHADER_PROGRAM_H
