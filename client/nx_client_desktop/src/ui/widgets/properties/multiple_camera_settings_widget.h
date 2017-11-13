#pragma once

#include <QtWidgets/QWidget>
#include "api/media_server_connection.h"
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"
#include "ui/workbench/workbench_context_aware.h"

namespace Ui {
class MultipleCameraSettingsWidget;
}

class QnMultipleCameraSettingsWidget :
    public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    explicit QnMultipleCameraSettingsWidget(QWidget *parent);
    ~QnMultipleCameraSettingsWidget();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    void updateFromResources();
    void submitToResources();

    /** Check if second stream is enabled if there are Motion+LQ tasks in schedule. */
    bool isValidSecondStream();

    bool hasDbChanges() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setExportScheduleButtonEnabled(bool enabled);

    void setLockedMode(bool locked);

signals:
    void hasChangesChanged();

private slots:
    void at_dbDataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_cameraScheduleWidget_scheduleEnabledChanged(int state);

private:
    void setHasDbChanges(bool hasChanges);
    void updateAlertBar();

    int tabIndex(Qn::CameraSettingsTab tab) const;
    void setTabEnabledSafe(Qn::CameraSettingsTab tab, bool enabled);

private:
    Q_DISABLE_COPY(QnMultipleCameraSettingsWidget)

    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;

    QnVirtualCameraResourceList m_cameras;
    bool m_hasDbChanges;

    bool m_loginWasEmpty;
    bool m_passwordWasEmpty;

    /** Indicates that the user changed controls but not applied them to the schedule */
    bool m_hasScheduleControlsChanges;

    bool m_readOnly;

    bool m_updating;

    QString m_alertText;
    bool m_lockedMode = false;
};
