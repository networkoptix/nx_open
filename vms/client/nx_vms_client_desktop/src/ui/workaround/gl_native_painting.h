#pragma once

class QPainter;
class QOpenGLWidget;

class QnGlNativePainting
{
public:
    static void begin(QOpenGLWidget* glWidget, QPainter* painter);
    static void end(QPainter* painter);
};
