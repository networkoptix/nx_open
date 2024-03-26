// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <QtCore/QObject>
#include <QtCore/QSize>

#include <nx/utils/thread/mutex.h>

class QOpenGLFramebufferObject;

namespace nx::media {

/**
 * This class stores frame buffer objects for decoded video frames.
 */
class FboManager: public QObject
{
    Q_OBJECT

    static constexpr int kFboPoolSize = 4;

public:
    struct FboHolder
    {
        FboHolder(std::weak_ptr<QOpenGLFramebufferObject> fbo, FboManager* manager);
        ~FboHolder();

        std::weak_ptr<QOpenGLFramebufferObject> fbo;
        FboManager* manager = nullptr;
    };

    using FboPtr = std::shared_ptr<FboHolder>;

    FboManager(const QSize& frameSize);

    virtual ~FboManager() override;

    void deleteWhenEmptyInThread(QThread* thread);

    FboPtr getFbo();

private:
    std::vector<std::shared_ptr<QOpenGLFramebufferObject>> m_fbos;
    std::queue<std::shared_ptr<QOpenGLFramebufferObject>> m_fbosToDelete;
    mutable Mutex m_mutex;
    QSize m_frameSize;
    bool m_deleteWhenEmpty = false;
};

} // namespace nx::media
