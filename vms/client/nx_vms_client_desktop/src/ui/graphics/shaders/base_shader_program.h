#pragma once

#include <QtGui/QOpenGLShaderProgram>

class QnGLShaderProgram : public QOpenGLShaderProgram
{
    Q_OBJECT

public:
    void setModelViewProjectionMatrix(const QMatrix4x4& value);

    virtual bool compile() { return false; };

    virtual bool link() override
    {
        const bool rez = QOpenGLShaderProgram::link();
        if (rez)
            m_modelViewProjection = uniformLocation("uModelViewProjectionMatrix");

        return rez;
    }

    void markInitialized() { m_initialized = true; };
    bool initialized() const { return m_initialized; };

protected:
    using QOpenGLShaderProgram::QOpenGLShaderProgram;

private:
    int m_modelViewProjection = -1;
    bool m_initialized = false;
};
