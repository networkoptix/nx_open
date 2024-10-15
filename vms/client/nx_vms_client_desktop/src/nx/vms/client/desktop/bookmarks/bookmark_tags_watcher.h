// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/camera_bookmark.h>
#include <nx/vms/client/desktop/system_context_aware.h>

class QTimer;

namespace nx::vms::client::desktop {

// TODO: #ynikitenkov Think about usage only on bookmarks dialog showing
class BookmarkTagsWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
public:
    explicit BookmarkTagsWatcher(SystemContext* systemContext, QObject* parent = nullptr);

    const QnCameraBookmarkTagList& tags() const;

private:
    void refresh();
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

private:
    QnCameraBookmarkTagList m_tags;
    QTimer* const m_refreshTimer;
};

} // namespace nx::vms::client::desktop
