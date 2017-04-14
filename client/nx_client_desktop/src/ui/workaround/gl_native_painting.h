#pragma once

class QPainter;
class QGLContext;

class QnGlNativePainting
{
public:
    static void begin(const QGLContext* context, QPainter* painter);
    static void end(QPainter* painter);
};
