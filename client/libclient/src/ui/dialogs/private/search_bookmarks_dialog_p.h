#pragma once

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>

#include <ui/workbench/workbench_context_aware.h>

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

    void updateHeadersWidth();

    void refresh();

    void setParameters(const QString &filterText
        , qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs);

private:
    QnVirtualCameraResourceList availableCameras() const;

    QnVirtualCameraResourcePtr availableCameraByUniqueId(const QString &uniqueId) const;

    void resetToAllAvailableCameras();

    void setCameras(const QnVirtualCameraResourceList &cameras);

    void chooseCamera();

    void customContextMenuRequested();

    void openInNewLayout(const QnActionParameters &params, const QnTimePeriod &window);

    bool fillActionParameters(QnActionParameters &params, QnTimePeriod &window);

    bool currentUserHasAdminPrivileges();

    void applyModelChanges();

private:
    typedef QScopedPointer<Ui::BookmarksLog> UiImpl;

    QDialog * const m_owner;
    const UiImpl m_ui;
    QnSearchBookmarksModel * const m_model;

    bool m_allCamerasChoosen;

    QAction * const m_openInNewTabAction;
    QAction * const m_editBookmarkAction;
    QAction * const m_exportBookmarkAction;
    QAction * const m_removeBookmarksAction;
    bool m_updatingNow;
};