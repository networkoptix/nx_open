// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fbo_manager.h"

#include <QtOpenGL/QOpenGLFramebufferObject>

namespace nx::media {

FboManager::FboHolder::FboHolder(std::weak_ptr<QOpenGLFramebufferObject> fbo, FboManager* manager):
    fbo(fbo),
    manager(manager)
{
    NX_ASSERT(!manager->m_deleteWhenEmpty);
}

FboManager::FboHolder::~FboHolder()
{
    const auto fboPtr = fbo.lock();
    if (!fboPtr)
        return;

    NX_MUTEX_LOCKER lock(&manager->m_mutex);
    manager->m_fbos.erase(
        std::find(manager->m_fbos.begin(), manager->m_fbos.end(), fboPtr));
    manager->m_fbosToDelete.push(fboPtr);
    if (manager->m_deleteWhenEmpty && manager->m_fbos.empty())
        manager->deleteLater();
}

FboManager::FboManager(const QSize& frameSize):
    m_frameSize(frameSize)
{
}

FboManager::~FboManager()
{
}

void FboManager::deleteWhenEmptyInThread(QThread* thread)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_deleteWhenEmpty = true;

    if (m_fbos.empty())
        deleteLater();
    else
        moveToThread(thread);
}

FboManager::FboPtr FboManager::getFbo()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    // It looks like the video surface we use releases the presented QVideoFrame too early
    // (before it is actually rendered). If we delete it immediately, we'll get screen
    // blinking. Here we give the FBO a slight delay defore the deletion.
    while (m_fbosToDelete.size() > 1)
        m_fbosToDelete.pop();

    m_fbos.emplace_back(new QOpenGLFramebufferObject(m_frameSize));
    return std::make_shared<FboHolder>(m_fbos.back(), this);
}

} // namespace nx::media
