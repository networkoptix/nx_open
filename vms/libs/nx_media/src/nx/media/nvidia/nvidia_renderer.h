// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>
#include <QtGui/QOpenGLFunctions>

class AbstractVideoSurface;

namespace nx::media::nvidia {

bool renderToRgb(AbstractVideoSurface* frame, GLuint textureId, const QSize& textureSize);

} // namespace nx::media::nvidia
