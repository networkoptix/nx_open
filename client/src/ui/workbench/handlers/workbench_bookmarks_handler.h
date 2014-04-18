#ifndef WORKBENCH_BOOKMARKS_HANDLER_H
#define WORKBENCH_BOOKMARKS_HANDLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark.h>

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
private slots:
    void at_bookmarkTimeSelectionAction_triggered();

    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_camera_bookmarkAdded(const QnSecurityCamResourcePtr &camera, const QnCameraBookmark &bookmark);
    void at_camera_bookmarkChanged(const QnSecurityCamResourcePtr &camera, const QnCameraBookmark &bookmark);
    void at_camera_bookmarkRemoved(const QnSecurityCamResourcePtr &camera, const QnCameraBookmark &bookmark);

private:
    typedef QHash<QString, int>  TagsUsageHash;
    QHash<QnSecurityCamResourcePtr, TagsUsageHash> m_tagsUsageByCamera;
    TagsUsageHash m_tagsUsage;
};

#endif // WORKBENCH_BOOKMARKS_HANDLER_H
