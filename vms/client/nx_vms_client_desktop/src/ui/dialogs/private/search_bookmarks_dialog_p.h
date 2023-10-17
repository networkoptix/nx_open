// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

class QDialog;

class QnSearchBookmarksModel;

namespace Ui {
    class BookmarksLog;
}

class QnSearchBookmarksDialogPrivate : public QObject , public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    QnSearchBookmarksDialogPrivate(const QString &filterText
        , qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs
        , QDialog *owner);

    ~QnSearchBookmarksDialogPrivate();

    void refresh();

    void setParameters(const QString &filterText
        , qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs);

    void cancelUpdateOperation();

private:
    QnVirtualCameraResourceList availableCameras() const;

    QnVirtualCameraResourcePtr availableCameraById(const QnUuid &cameraId) const;

    void resetToAllAvailableCameras();

    void setCameras(const QnVirtualCameraResourceList &cameras);

    void chooseCamera();

    void customContextMenuRequested();

    void openInNewLayout(const nx::vms::client::desktop::menu::Parameters& params,
        const QnTimePeriod &window);

    bool fillActionParameters(nx::vms::client::desktop::menu::Parameters &params, QnTimePeriod &window);

    void applyModelChanges();

    void reset();

private:
    typedef QScopedPointer<Ui::BookmarksLog> UiImpl;

    QDialog * const m_owner;
    const UiImpl ui;
    QnSearchBookmarksModel * const m_model;

    bool m_allCamerasChoosen;

    QAction* const m_openInNewTabAction;
    QAction* const m_editBookmarkAction;
    QAction* const m_exportBookmarkAction;
    QAction* const m_exportBookmarksAction;
    QAction* const m_copyBookmarkTextAction;
    QAction* const m_copyBookmarksTextAction;
    QAction* const m_removeBookmarkAction;
    QAction* const m_removeBookmarksAction;
    bool m_updatingParametersNow;
    bool m_columnResizeRequired;

    const qint64 utcRangeStartMs;
    const qint64 utcRangeEndMs;
};
