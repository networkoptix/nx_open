// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QPainter;
class QOpenGLWidget;

class QnGlNativePainting
{
public:
    static void begin(QOpenGLWidget* glWidget, QPainter* painter);
    static void end(QPainter* painter);
};
