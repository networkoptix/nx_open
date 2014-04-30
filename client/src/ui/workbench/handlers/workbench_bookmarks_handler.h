#ifndef WORKBENCH_BOOKMARKS_HANDLER_H
#define WORKBENCH_BOOKMARKS_HANDLER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

/**
 * @brief The QnWorkbenchBookmarksHandler class         Handler for camera bookmarks management.
 */
class QnWorkbenchBookmarksHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchBookmarksHandler(QObject *parent = NULL);

    QnCameraBookmarkTags tagsUsage() const;
private slots:
    void at_bookmarkTimeSelectionAction_triggered();
    void at_bookmarkAdded(int status, const QnCameraBookmark &bookmark, int handle);

    void updateTagsUsage();
private:
    ec2::AbstractECConnectionPtr connection() const;
    QnMediaServerResourcePtr getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 time) const;

    QnCameraBookmarkTags m_tagsUsage;
    QHash<int, QnResourcePtr> m_addingBookmarks; 
};

#endif // WORKBENCH_BOOKMARKS_HANDLER_H
