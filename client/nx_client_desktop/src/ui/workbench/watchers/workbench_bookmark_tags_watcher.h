#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>


// TODO: #ynikitenkov Thionk about usage only on bookmarks dialog showing
class QnWorkbenchBookmarkTagsWatcher : public QObject, public QnConnectionContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchBookmarkTagsWatcher(QObject *parent = 0);

    QnCameraBookmarkTagList tags() const;

signals:
    void tagsChanged(const QnCameraBookmarkTagList &tags);

public slots:
    void refresh();

private slots:
    void at_connection_gotBookmarkTags(int status, const QnCameraBookmarkTagList &tags, int handle);
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

private:
    QnCameraBookmarkTagList m_tags;
    QTimer *m_refreshTimer;
};
