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

    QnCameraBookmarkTags tags() const;
private slots:
    void at_addCameraBookmarkAction_triggered();
    void at_editCameraBookmarkAction_triggered();
    void at_removeCameraBookmarkAction_triggered();

    void updateTags();
private:
    ec2::AbstractECConnectionPtr connection() const;

    QnCameraBookmarkTags m_tags;
};

#endif // WORKBENCH_BOOKMARKS_HANDLER_H
