// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>

#include <QtCore/QSharedPointer>
#include <QtGui/QOpenGLContext>

class QOpenGLWidget;
class QnGlFunctionsPrivate;

#ifndef GL_ARB_vertex_buffer_object
typedef std::ptrdiff_t GLintptrARB;
typedef std::ptrdiff_t GLsizeiptrARB;
#endif

#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif

typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;

class QnGlFunctions
{
    friend class QnGraphicsView;

public:
    enum Feature
    {
        // Shaders are not supported because of the issues with the 3rd-party software.
        ShadersBroken       = 0x10000000,
        NoOpenGLFullScreen  = 0x20000000    /**< There are some artifacts in fullscreen mode, so we shouldn't go to fullscreen. */
    };
    Q_DECLARE_FLAGS(Features, Feature);

    /**
     * Constructor.
     *
     * \param context                   OpenGL context that this functions instance will work with.
     */
    QnGlFunctions(QOpenGLWidget* glWidget);

    /**
     * Virtual destructor.
     */
    virtual ~QnGlFunctions();

    /**
     * \returns                         Set of features supported by the current OpenGL context.
     */
    Features features() const;

    /**
     * \brief                           Set of OpenGL plain-text params.
     */
    struct OpenGLInfo { QString version, vendor, renderer, extensions; };

    /**
     * \returns                         Actual \class OpenGLInfo setting.
     */
    static OpenGLInfo openGLInfo();

    QOpenGLWidget* glWidget() const;

    /**
     * \returns                         Set of features supported by the current OpenGL context. Estimated BEFORE context initializing.
     */
    static Features estimatedFeatures();

    /**
     * \param target                    Value to estimate. Only <tt>GL_MAX_TEXTURE_SIZE</tt> is currently supported.
     * \returns                         Estimated value. It is not guaranteed to be equal to the real value obtained
     *                                  via <tt>glGetIntegerv</tt> function, but it will be safe to use.
     */
    static GLint estimatedInteger(GLenum target);

    /**
     * \param enable                    Whether warnings related to unsupported function calls are to be enabled.
     */
    static void setWarningsEnabled(bool enable);

    /**
     * \returns                         Whether warnings related to unsupported function calls are enabled.
     */
    static bool isWarningsEnabled();

private:
    static void overrideMaxTextureSize(int size);

    using PrivatePtr = QSharedPointer<QnGlFunctionsPrivate>;
    static PrivatePtr createPrivate(QOpenGLWidget* glWidget);
    PrivatePtr d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnGlFunctions::Features)
