// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

/**
 * @brief The QnWorkbenchBookmarksHandler class         Handler for camera bookmarks management.
 */
class QnWorkbenchBookmarksHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchBookmarksHandler(QObject *parent = nullptr);
    virtual ~QnWorkbenchBookmarksHandler();

private:
    void at_addCameraBookmarkAction_triggered();
    void at_editCameraBookmarkAction_triggered();
    void at_removeCameraBookmarkAction_triggered();
    void at_removeBookmarksAction_triggered();
    void at_bookmarksModeAction_triggered();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
