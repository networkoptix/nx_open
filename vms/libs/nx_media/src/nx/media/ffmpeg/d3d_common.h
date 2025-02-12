// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <comdef.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <chrono>
#include <stdexcept>
#include <vector>

#include <QtCore/QElapsedTimer>
#include <QtCore/QSize>

#include <nx/utils/log/format.h>


static inline void throwIfFailedWithLocation(
    HRESULT hr, const char* text, const char* func, const char* file, int line)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(
            nx::format("Failed with HRESULT=0x%1 (%2) %3 at %4:%5",
                QString::number((uint32_t) hr, 16),
                std::system_category().message(hr),
                text,
                file,
                line).toStdString());
    }
}

#define throwIfFailed(hr) throwIfFailedWithLocation((hr), #hr, __FUNCTION__, __FILE__, __LINE__)

namespace nx::media {

template<typename D>
concept TextureDesc = requires(D d)
{
    { d.Format } -> std::convertible_to<DXGI_FORMAT>;
};

template<typename T>
concept TextureData = requires(T t)
{
    { t.format } -> std::convertible_to<DXGI_FORMAT>;
    { t.size } -> std::convertible_to<QSize>;
};

using namespace std::chrono;

// A pool of shared textures for copying video frames from FFmpeg to RHI. This template handles
// common logic for a texture pool and requires a derived class to implement the creation of new
// texture - either ID3D11Texture2D or ID3D12Resource.
// The pool is used to avoid creating and destroying textures for each frame, which is expensive.
// To avoid wasting memory, the pool size is adjusted based on the video frame rate.
template<typename Derived, TextureData T>
class TexturePoolBase
{
    static constexpr auto kBufferDuration = 200ms; //< Buffer duration in QnBufferedFrameDisplayer.
    static constexpr size_t kFramesInFlight = 4; //< Approximate number of frames in other buffers.
    static constexpr size_t kMinTextureCount = kFramesInFlight; //< 1 fps.
    static constexpr size_t kDefaultTextureCount = kFramesInFlight + 6; //< 30 fps.
    static constexpr size_t kMaxTextureCount = kFramesInFlight + 12; //< 60 fps.

public:
    // Returns a shared texture from the pool. This method is always called from the decoder thread
    // and can throttle the decoder if the pool is full.
    template<TextureDesc D>
    std::shared_ptr<T> getTexture(const D& desc, const QSize& size)
    {
        updateMaxTextureCount();

        const auto format = desc.Format;

        while (true)
        {
            // Ensure the pool is not exceeding the maximum size.
            if (m_textures.size() <= m_maxTextureCount
                || removeAtMost(m_textures.size() - m_maxTextureCount))
            {
                // Look for an unused texture.
                for (auto& tex: m_textures)
                {
                    if (unused(tex))
                    {
                        if (tex->size != size || tex->format != format)
                            tex = static_cast<Derived*>(this)->newTexture(desc, size);
                        return tex;
                    }
                }

                // Create a new texture if the pool is not full.
                if (m_textures.size() < m_maxTextureCount)
                {
                    m_textures.push_back(static_cast<Derived*>(this)->newTexture(desc, size));
                    return m_textures.back();
                }
            }

            // Wait for the next frame to be rendered and one of the textures to be released.
            // Win32 Sleep() is not very accurate so use a minimal sleep time.
            Sleep(1);
        }
    }

private:
    static bool unused(const std::shared_ptr<T>& tex)
    {
        return tex.use_count() <= 1;
    }

    // Removes at most count unused textures from the pool. Returns true if all requested textures
    // were removed.
    bool removeAtMost(size_t count)
    {
        size_t remaining = std::min(count, m_textures.size());

        m_textures.erase(
            std::remove_if(m_textures.begin(), m_textures.end(),
                [&remaining](const auto& tex)
                {
                    const bool okToRemove = unused(tex) && remaining > 0;
                    if (okToRemove)
                        --remaining;
                    return okToRemove;
                }),
            m_textures.end());

        return remaining == 0;
    }

    // Adjust the maximum texture count based on the frame rate.
    void updateMaxTextureCount()
    {
        if (!m_fpsTimer.isValid())
            m_fpsTimer.start();

        ++m_frameCount;

        if (m_fpsTimer.elapsed() > 1000)
        {
            const float bufferDurationMs = duration_cast<milliseconds>(kBufferDuration).count();
            const float framesInBuffer = (bufferDurationMs * m_frameCount) / m_fpsTimer.elapsed();

            m_fpsTimer.restart();
            m_frameCount = 0;

            const auto newTextureCount = std::clamp<int>(
                std::ceil(framesInBuffer + kFramesInFlight),
                kMinTextureCount,
                kMaxTextureCount);

            if (std::abs(newTextureCount - (int) m_maxTextureCount) > 1)
                m_maxTextureCount = newTextureCount;
        }
    }

protected:
    std::vector<std::shared_ptr<T>> m_textures;
    size_t m_maxTextureCount = kDefaultTextureCount;
    QElapsedTimer m_fpsTimer;
    size_t m_frameCount = 0;
};

} // namespace nx::media
