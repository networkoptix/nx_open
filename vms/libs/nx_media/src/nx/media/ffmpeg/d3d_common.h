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
            nx::format("Failed with HRESULT=0x%1 (%2) %3 at %4(%5):%6",
                QString::number((uint32_t) hr, 16),
                std::system_category().message(hr),
                text,
                file,
                func,
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
    static constexpr size_t kDefaultTexturePoolSize = 4;

public:
    // Returns a shared texture from the pool. This method is always called from the decoder thread
    template<TextureDesc D>
    std::shared_ptr<T> getTexture(const D& desc, const QSize& size)
    {
        if (m_textures.size() > kDefaultTexturePoolSize)
            removeAtMost(m_textures.size() - kDefaultTexturePoolSize);


        // Look for an unused texture.
        const auto format = desc.Format;
        for (auto& tex: m_textures)
        {
            if (unused(tex))
            {
                if (tex->size != size || tex->format != format)
                    tex = static_cast<Derived*>(this)->newTexture(desc, size);
                return tex;
            }
        }

        m_textures.push_back(static_cast<Derived*>(this)->newTexture(desc, size));
        return m_textures.back();
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

protected:
    std::vector<std::shared_ptr<T>> m_textures;
};

} // namespace nx::media
