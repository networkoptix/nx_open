#ifndef QN_GL_FUNCTIONS_H
#define QN_GL_FUNCTIONS_H

#include <cstddef> /* For std::ptrdiff_t. */

#include <QtOpenGL/QGLContext>

class QGLContext;

class QnGlFunctionsPrivate;

#ifndef GL_ARB_vertex_buffer_object
typedef std::ptrdiff_t GLintptrARB;
typedef std::ptrdiff_t GLsizeiptrARB;
#endif

#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif


class QnGlFunctions {
public:
    enum Feature {
        ArbPrograms = 0x1,              /**< Supports ARB shader programs. */
        OpenGL1_3 = 0x2,                /**< Implements OpenGL1.3 spec. */
        OpenGL1_5 = 0x4,                /**< Implements OpenGL1.5 spec. */
        OpenGL2_0 = 0x8,                /**< Implements OpenGL2.0 spec. */
        
        ShadersBroken = 0x00010000,     /**< Vendor has messed something up, and shaders are not supported. */
        NoOpenGLFullScreen = 0x0002000  /**< There are some artifacts in fullscreen mode, so we shouldn't go to fullscreen. */
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
    QGLContext *context() const;

    /**
     * \returns                         Set of features supported by the current OpenGL context.
     */
    Features features() const;

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

    /* ArbPrograms group. */

    void glProgramStringARB(GLenum target, GLenum format, GLsizei len, const GLvoid *string) const;
    void glBindProgramARB(GLenum target, GLuint program) const;
    void glDeleteProgramsARB(GLsizei n, const GLuint *programs) const;
    void glGenProgramsARB(GLsizei n, GLuint *programs) const;
    void glProgramLocalParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) const;

    /* OpenGL1_3 group. */

    void glActiveTexture(GLenum texture);

    /* OpenGL1_5 group. */
    
    void glGenBuffers(GLsizei n, GLuint *buffers);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glDeleteBuffers(GLsizei n, const GLuint *buffers);
    void glBufferData(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);

    /* OpenGL2_0 group. */
    
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
    void glEnableVertexAttribArray(GLuint index);
    void glDisableVertexAttribArray(GLuint index);

private:
    QSharedPointer<QnGlFunctionsPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnGlFunctions::Features);


#ifndef GL_FRAGMENT_PROGRAM_ARB
#   define GL_FRAGMENT_PROGRAM_ARB              0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII_ARB
#   define GL_PROGRAM_FORMAT_ASCII_ARB          0x8875
#endif

#ifndef GL_CLAMP_TO_EDGE
#   define GL_CLAMP_TO_EDGE                     0x812F
#endif

#ifndef GL_TEXTURE0
#   define GL_TEXTURE0                          0x84C0
#endif 
#ifndef GL_TEXTURE1
#   define GL_TEXTURE1                          0x84C1
#endif
#ifndef GL_TEXTURE2
#   define GL_TEXTURE2                          0x84C2
#endif

#ifndef GL_VERSION_1_5
#   define GL_ARRAY_BUFFER                      0x8892
#   define GL_ELEMENT_ARRAY_BUFFER              0x8893
#   define GL_STREAM_DRAW                       0x88E0
#   define GL_STREAM_READ                       0x88E1
#   define GL_STREAM_COPY                       0x88E2
#   define GL_STATIC_DRAW                       0x88E4
#   define GL_STATIC_READ                       0x88E5
#   define GL_STATIC_COPY                       0x88E6
#   define GL_DYNAMIC_DRAW                      0x88E8
#   define GL_DYNAMIC_READ                      0x88E9
#   define GL_DYNAMIC_COPY                      0x88EA
#endif

#endif // QN_GL_FUNCTIONS_H
