#ifndef QN_GL_FUNCTIONS_H
#define QN_GL_FUNCTIONS_H

#include <cstddef> /* For std::ptrdiff_t. */

#include <QtOpenGL/QGLContext>

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


class QnGlFunctions {
public:
    enum Feature {
        ShadersBroken       = 0x10000000,   /**< Vendor has messed something up, and shaders are not supported. */
        NoOpenGLFullScreen  = 0x20000000    /**< There are some artifacts in fullscreen mode, so we shouldn't go to fullscreen. */
    };
    Q_DECLARE_FLAGS(Features, Feature);

    /**
     * Constructor.
     * 
     * \param context                   OpenGL context that this functions instance will work with. 
     */
    QnGlFunctions(const QGLContext *context);

    /**
     * Virtual destructor.
     */
    virtual ~QnGlFunctions();

    /**
     * \returns                         OpenGL context that this functions instance works with.
     */
    const QGLContext *context() const;

    /**
     * \returns                         Set of features supported by the current OpenGL context.
     */
    Features features() const;

	/**
	 * \brief                           Set of OpenGL plain-text params.
	 */
	struct OpenGLInfo { QString version, vendor, renderer; };

	/**
     * \returns                         Actual \class OpenGLInfo setting.
     */
	const OpenGLInfo& openGLInfo() const;

	/**
     * \returns                         Returns last (cached) \class OpenGLInfo setting (might be empty).
     */
	static OpenGLInfo openGLCachedInfo();

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
    QSharedPointer<QnGlFunctionsPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnGlFunctions::Features)

#endif // QN_GL_FUNCTIONS_H
