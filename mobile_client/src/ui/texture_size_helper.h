#pragma once

#include <QtCore/QObject>
#include <utils/thread/mutex.h>
#include <utils/common/singleton.h>

class QQuickWindow;

class QnTextureSizeHelper : public QObject, public Singleton<QnTextureSizeHelper> {
    Q_OBJECT

public:
    explicit QnTextureSizeHelper(QQuickWindow *window, QObject *parent = 0);

    int maxTextureSize() const;

private:
    mutable QnMutex m_mutex;
    int m_maxTextureSize;
};
