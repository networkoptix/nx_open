#ifndef QN_GL_FUNCTIONS_H
#define QN_GL_FUNCTIONS_H

#include <QtOpenGL>

class QGLContext;

class QnGlFunctionsPrivate;

class QnGlFunctions {
public:
    enum Feature {
        ArbPrograms = 0x1,          /**< Supports ARB shader programs. */
        OpenGL1_3 = 0x2,            /**< Implements OpenGL1.3 spec. */
        ShadersBroken = 0x4,        /**< Vendor has messed something up, and shaders are not supported. */
        OpenGLBroken = 0x8          /**< Vendor has messed something up, and videodriver dies using OpenGL. */
    };
    Q_DECLARE_FLAGS(Features, Feature);

    /**
     * Constructor.
     * 
     * \param context                   OpenGL context that this functions instance will work with. 
     *                                  If NULL, current context will be used.
     */
    QnGlFunctions(const QGLContext *context = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnGlFunctions();

    /**
     * \returns                         Set of features supported by the current OpenGL context.
     */
    Features features() const;

    /**
     * \returns                         Set of features supported by the current OpenGL context. Estimated BEFORE context initializing.
     */
    static Features estimatedFeatures();

    /**
     * \param enable                    Whether warnings related to unsupported function calls are to be enabled.
     */
    static void enableWarnings(bool enable);

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
#ifndef GL_CLAMP_TO_EDGE_SGIS
#   define GL_CLAMP_TO_EDGE_SGIS                0x812F
#endif
#ifndef GL_CLAMP_TO_EDGE_EXT
#   define GL_CLAMP_TO_EDGE_EXT                 0x812F
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


#endif // QN_GL_FUNCTIONS_H
