#ifndef QN_GL_NATIVE_PAINTING_H
#define QN_GL_NATIVE_PAINTING_H

class QPainter;

class QnGlNativePainting {
public:
    static void begin(const QGLContext* context,QPainter *painter);
    static void end(QPainter *painter);
};

#endif // QN_GL_NATIVE_PAINTING_H
