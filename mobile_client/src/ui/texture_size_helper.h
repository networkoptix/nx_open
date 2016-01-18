#pragma once

#include <QtCore/QObject>
#include <utils/thread/mutex.h>
#include <utils/common/singleton.h>

class QQuickWindow;

class QnTextureSizeHelper : public QObject, public Singleton<QnTextureSizeHelper> {
    Q_OBJECT

public:
    explicit QnTextureSizeHelper(QQuickWindow *window, QObject *parent = 0);
    ~QnTextureSizeHelper();

    int maxTextureSize() const;

    static int createTexture();

private:
    mutable QnMutex m_mutex;
    int m_maxTextureSize;
    QQuickWindow *m_window;
};
