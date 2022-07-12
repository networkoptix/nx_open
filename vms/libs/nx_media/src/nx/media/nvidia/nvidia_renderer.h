// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QOpenGLContext>
#include <QtMultimedia/QVideoFrame>

class AbstractVideoSurface;

bool renderToRgb(
    AbstractVideoSurface* frame,
    bool isNewTexture,
    GLuint textureId,
    QOpenGLContext* /*context*/,
    int scaleFactor,
    float* cropWidth,
    float* cropHeight);