#ifndef QN_NV12_TO_RGB_SHADER_PROGRAM_H
#define QN_NV12_TO_RGB_SHADER_PROGRAM_H



#include <QtOpenGL/QGLShaderProgram>
#include "base_shader_program.h"

class QnNv12ToRgbShaderProgram: public QnGLShaderProgram {
    Q_OBJECT
public:
    enum Colorspace {
        YuvEbu,
        YuvBt601,
        YuvBt709,
        YuvSmtp240m
    };

    QnNv12ToRgbShaderProgram(const QGLContext *context, QObject *parent = NULL);

    /**
     * \param target                    Number of the texture block that Y-plane is loaded to. 
     *                                  It is presumed to be in <tt>GL_LUMINANCE</tt> format.
     */
    void setYTexture(int target) {
        setUniformValue(m_yTextureLocation, target);
    }

    /**
     * \param target                    Number of the texture block that UV-plane is loaded to.
     *                                  It is presumed to be in <tt>GL_LUMINANCE_ALPHA</tt> format, 
     *                                  U component stored as luminance, and V as alpha.
     */
    void setUVTexture(int target) {
        setUniformValue(m_uvTextureLocation, target);
    }

    /**
     * \param transform                 YUV->RGB colorspace transformation to use. 
     */
    void setColorTransform(const QMatrix4x4 &transform) {
        setUniformValue(m_colorTransformLocation, transform);
    }

    void setOpacity(GLfloat opacity) {
        setUniformValue(m_opacityLocation, opacity);
    }

    static QMatrix4x4 colorTransform(Colorspace colorspace, bool fullRange = true);
    bool wasLinked(){ return m_wasLinked; };
private:
    int m_yTextureLocation;
    int m_uvTextureLocation;
    int m_colorTransformLocation;
    int m_opacityLocation;
    bool m_wasLinked;
};


#endif // QN_NV12_TO_RGB_SHADER_PROGRAM_H
