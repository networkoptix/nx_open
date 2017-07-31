#pragma once

namespace nx {
namespace sdk {
namespace v0 {

enum class FrameHandleType
{
    none,
    any,
    glTexture,
    xVideoSharedMemory,
    coreImage,
    qPixmap,
    eglImage
};

} // namespace v0
} // namespace sdk
} // namespace nx
