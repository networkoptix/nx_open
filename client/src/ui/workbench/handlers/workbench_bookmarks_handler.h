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

    QnCameraBookmarkTagsUsage tagsUsage() const;
private slots:
    void at_bookmarkTimeSelectionAction_triggered();
    void at_bookmarkAdded(int status, const QVariant &data, int handle);

    void updateTagsUsage();
private:
    QnMediaServerResourcePtr getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 time) const;

    QnCameraBookmarkTagsUsage m_tagsUsage;
    QHash<int, QnResourcePtr> m_addingBookmarks; 
};

#endif // WORKBENCH_BOOKMARKS_HANDLER_H
