// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

class QTimer;

// TODO: #ynikitenkov Thionk about usage only on bookmarks dialog showing
class QnWorkbenchBookmarkTagsWatcher:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchBookmarkTagsWatcher(QObject* parent = nullptr);

    QnCameraBookmarkTagList tags() const;

signals:
    void tagsChanged(const QnCameraBookmarkTagList &tags);

public slots:
    void refresh();

private slots:
    void at_connection_gotBookmarkTags(bool success, const QnCameraBookmarkTagList &tags);
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

private:
    QnCameraBookmarkTagList m_tags;
    QTimer *m_refreshTimer;
};
