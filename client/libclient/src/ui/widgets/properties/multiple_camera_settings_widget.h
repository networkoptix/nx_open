#pragma once

#include <QtWidgets/QWidget>
#include "api/media_server_connection.h"
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"
#include "ui/workbench/workbench_context_aware.h"
#include <ui/common/updatable.h>

namespace Ui {
class MultipleCameraSettingsWidget;
}

class QnCameraSettingsWidgetPrivate;

class QnMultipleCameraSettingsWidget : public QWidget, public QnWorkbenchContextAware, public QnUpdatable
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

    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const;

    //!Return true, if some parameter(s), requiring license validation has(-ve) been changed
    bool licensedParametersModified() const;
    void updateFromResources();
    void submitToResources();
    void reject();

    /** Check if second stream is enabled if there are Motion+LQ tasks in schedule. */
    bool isValidSecondStream();

    bool hasDbChanges() const
    {
        return m_hasDbChanges;
    }

    /** Checks if user changed controls but not applied them to the schedule */
    bool hasScheduleControlsChanges() const
    {
        return m_hasScheduleControlsChanges;
    }

    /** Clear flag that user changed schedule controls but not applied them */
    void clearScheduleControlsChanges()
    {
        m_hasScheduleControlsChanges = false;
    }

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setExportScheduleButtonEnabled(bool enabled);
signals:
    void hasChangesChanged();

    private slots:
    void at_dbDataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_cameraScheduleWidget_scheduleEnabledChanged(int state);

    void updateMaxFPS();

protected:
    QnCameraSettingsWidgetPrivate* d_ptr;

private:
    void setHasDbChanges(bool hasChanges);

    int tabIndex(Qn::CameraSettingsTab tab) const;
    void setTabEnabledSafe(Qn::CameraSettingsTab tab, bool enabled);

private:
    Q_DISABLE_COPY(QnMultipleCameraSettingsWidget)
    Q_DECLARE_PRIVATE(QnCameraSettingsWidget)

    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;

    QnVirtualCameraResourceList m_cameras;
    bool m_hasDbChanges;
    bool m_hasScheduleChanges;
    bool m_hasScheduleEnabledChanges;

    bool m_loginWasEmpty;
    bool m_passwordWasEmpty;

    /** Indicates that the user changed controls but not applied them to the schedule */
    bool m_hasScheduleControlsChanges;

    bool m_readOnly;
    bool m_inUpdateMaxFps;
};
