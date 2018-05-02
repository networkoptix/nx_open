#pragma once

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

private slots:
    void at_addCameraBookmarkAction_triggered();
    void at_editCameraBookmarkAction_triggered();
    void at_removeCameraBookmarkAction_triggered();
    void at_removeBookmarksAction_triggered();
    void at_bookmarksModeAction_triggered();

private:
    void setupBookmarksExport();

private:
    /** If 'Press Ctrl-B' hint was already displayed for the current user. */
    bool m_hintDisplayed;
};
