// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <GL/gl.h>

class AbstractVideoSurface;

bool renderToRgb(AbstractVideoSurface* frame, GLuint textureId, int textureWidth, int textureHeight);