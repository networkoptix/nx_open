// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fbo_manager.h"

#include <QtOpenGL/QOpenGLFramebufferObject>

namespace nx::media {

class FboTexture
{
    friend class FboHolder;

public:
    FboTexture(const QSize& size)
    {
        m_fbo = std::make_unique<QOpenGLFramebufferObject>(size);
    }

    virtual ~FboTexture()
    {
        if (m_textureId.has_value())
        {
            NX_ASSERT(!m_fbo, "Texture has been taken, but FBO is still alive");
            glDeleteTextures(1, &m_textureId.value());
        }
    }

    void releaseFbo()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_fbo)
        {
            m_textureId = m_fbo->takeTexture();
            m_fbo.reset();
        }
    }

    GLuint textureId() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_textureId)
            return *m_textureId;

        return NX_ASSERT(m_fbo) ? m_fbo->texture() : 0;
    }

    bool isValid() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_fbo && m_fbo->isValid();
    }

    bool inUse() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_inUse;
    }

    bool setInUse(bool value)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (!NX_ASSERT(m_inUse != value))
            return false;

        m_inUse = value;
        return true;
    }

private:
    std::unique_ptr<QOpenGLFramebufferObject> m_fbo;
    std::optional<GLuint> m_textureId;
    mutable Mutex m_mutex;
    bool m_inUse = false;
};

FboTextureHolder::FboTextureHolder(std::shared_ptr<FboTexture> fboTexture):
    m_fboTexture(std::move(fboTexture))
{
    if (m_fboTexture)
    {
        if (!m_fboTexture->setInUse(true))
            m_fboTexture.reset();
    }
}

FboTextureHolder::~FboTextureHolder()
{
    if (m_fboTexture)
        m_fboTexture->setInUse(false);
}

GLuint FboTextureHolder::textureId() const
{
    return m_fboTexture ? m_fboTexture->textureId() : 0;
}

FboHolder::FboHolder(const QSize& size)
{
    m_fboTexture = std::make_shared<FboTexture>(size);
}

FboHolder::~FboHolder()
{
    if (m_fboTexture)
        m_fboTexture->releaseFbo();
}

bool FboHolder::inUse() const
{
    return NX_ASSERT(m_fboTexture) && m_fboTexture->inUse();
}

bool FboHolder::isValid() const
{
    return m_fboTexture->isValid();
}

void FboHolder::bind()
{
    if (NX_ASSERT(m_fboTexture))
        m_fboTexture->m_fbo->bind();
}
void FboHolder::release()
{
    if (NX_ASSERT(m_fboTexture))
        m_fboTexture->m_fbo->release();
}

void FboManager::init(const QSize& frameSize)
{
    m_fbos.clear();
    m_frameSize = frameSize;
}

FboTextureHolder FboManager::getTexture(std::function<void(FboHolder&)> renderFunction)
{
    auto it = std::find_if(
        m_fbos.begin(),
        m_fbos.end(),
        [](const auto& fbo)
        {
            return !fbo.inUse();
        });

    if (it == m_fbos.end())
    {
        if (m_fbos.size() >= kFboPoolSize)
            return {};

        // Create new FBO.
        m_fbos.emplace_back(m_frameSize);
        it = m_fbos.end() - 1;

        // Check if FBO was created.
        if (!it->isValid())
        {
            m_fbos.pop_back();
            return {};
        }
    }

    renderFunction(*it);

    return it->getTexture();
}

} // namespace nx::media
