#pragma once

#include <QtCore/QObject>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>

class QnWorkbenchBookmarkTagsWatcher : public QObject {
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
