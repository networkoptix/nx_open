#pragma once

#include <memory>

#include <QtMultimedia/QAbstractVideoBuffer>

#include <va/va.h>

constexpr int kHandleTypeVaSurface = QAbstractVideoBuffer::UserHandle + 1;

struct VaSurfaceInfo
{
    VASurfaceID id;
    VADisplay display;
};
Q_DECLARE_METATYPE(VaSurfaceInfo);
