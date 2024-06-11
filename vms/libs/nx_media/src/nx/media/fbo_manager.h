// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtGui/QOpenGLFunctions>

#include <nx/utils/thread/mutex.h>

class QOpenGLFramebufferObject;

namespace nx::media {

/**
 * The Android HW video decoder code works in the following way: it decodes video frames into a
 * GL_TEXTURE_EXTERNAL_OES texture and then we paint that texture using an OpenGL frame buffer
 * object into a regular texture. We create the QVideoFrame with that regular texture and pass it
 * to QVideoSink for display.
 *
 * We want to avoid creating and destroying frame buffer objects and textures for each frame, so
 * we create a pool of frame buffer objects. Each frame buffer object has a texture attached to it.
 * The issue is a video frame with a texture (attached to frame buffer object) can outlive that
 * frame buffer object when the decoder (and its thread) is destroyed and video frames are still
 * being displayed. We cannot delay the destruction of the frame buffer object because it cannot be
 * shared between thread-bound OpenGL contexts, so it should be destroyed in the same thread.
 * Fortunately textures can be shared between contexts, so when that frame buffer object is
 * destroyed, we want to detach the texture from it and transfer ownership to the video frame.
 *
 * The code below implements that logic.
 *
 * FboTextureHolder and FboHolder are move-only classes that share the ownership of the FboTexture
 * object, which is either a frame buffer object with texture or a detached texture. FboHolders
 * are stored in a pool. Using a FboHolder and painting a frame from the decoder produces a
 * FboTextureHolder, which is then passed to QVideoFrame moved into QVideoSink. When QVideoFrame
 * is destroyed (after display), the frame buffer can be reused for the next frame. When FboHolder
 * goes out of scope, it just detaches the texture, allowing it to be destroyed when the last
 * reference to FboTexture (which is held by FboTextureHolder inside a video frame) is destroyed.
 */

class FboTexture;

/**
 * This class manages life cycle of a single texture. When it is destroyed, it marks the frame
 * buffer object texture as no longer used, allowing it to be reused for rendering the next frame.
 * However, if the frame buffer object was destroyed already, it destroys the texture instead.
 */
class FboTextureHolder
{
public:
    FboTextureHolder(std::shared_ptr<FboTexture> fboTexture = nullptr);
    virtual ~FboTextureHolder();

    FboTextureHolder(FboTextureHolder&& other) = default;
    FboTextureHolder& operator=(FboTextureHolder&&) = default;
    FboTextureHolder(const FboTextureHolder&) = delete;
    FboTextureHolder& operator=(const FboTextureHolder&) = delete;

    GLuint textureId() const;

    bool isNull() const
    {
        return m_fboTexture == nullptr;
    }

private:
    std::shared_ptr<FboTexture> m_fboTexture;
};

/**
 * This class manages life cycle of a single frame buffer object. When it is destroyed, it detaches
 * frame buffer texture and destroys the frame buffer object, allowing the texture to outlive the
 * frame buffer object.
 */
class FboHolder
{
public:
    FboHolder(const QSize& size);
    virtual ~FboHolder();

    FboHolder(FboHolder&& other) = default;
    FboHolder& operator=(FboHolder&&) = default;
    FboHolder(const FboHolder&) = delete;
    FboHolder& operator=(const FboHolder&) = delete;

    bool inUse() const;

    bool isValid() const;

    void bind();
    void release();

    FboTextureHolder getTexture() const { return {m_fboTexture}; }

private:
    std::shared_ptr<FboTexture> m_fboTexture;
};

/**
 * This class stores frame buffer objects for decoded video frames. The number of frame buffers
 * grows dynamically (when all present FBO textures are in use) up to kFboPoolSize.
 */
class FboManager
{
    // QVideoSink has a buffer of 4 video frames and 1 video frame is being processed. Make sure
    // there is at least 1 FBO which texture is not in use.
    static constexpr size_t kFboPoolSize = 6;

public:
    void init(const QSize& frameSize);

    FboTextureHolder getTexture(std::function<void(FboHolder&)> renderFunction);

private:
    std::vector<FboHolder> m_fbos;
    QSize m_frameSize;
};

} // namespace nx::media
