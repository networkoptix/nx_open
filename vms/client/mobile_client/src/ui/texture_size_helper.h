// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

class QQuickWindow;

class QnTextureSizeHelper
:
    public QObject,
    public Singleton<QnTextureSizeHelper>
{
    Q_OBJECT

public:
    explicit QnTextureSizeHelper(QQuickWindow *window, QObject *parent = 0);

    int maxTextureSize() const;

private:
    mutable nx::Mutex m_mutex;
    int m_maxTextureSize;
};
