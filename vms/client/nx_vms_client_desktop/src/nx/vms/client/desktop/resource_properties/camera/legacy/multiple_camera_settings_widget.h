#pragma once

#include <QtWidgets/QWidget>

#include <api/media_server_connection.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>

namespace Ui { class MultipleCameraSettingsWidget; }

namespace nx::vms::client::desktop {

class MultipleCameraSettingsWidget:
    public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    explicit MultipleCameraSettingsWidget(QWidget *parent);
    ~MultipleCameraSettingsWidget();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    CameraSettingsTab currentTab() const;
    void setCurrentTab(CameraSettingsTab tab);

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
    void at_cameraScheduleWidget_scheduleEnabledChanged(int state);

private:
    void setHasDbChanges(bool hasChanges);
    void updateAlertBar();

    int tabIndex(CameraSettingsTab tab) const;
    void setTabEnabledSafe(CameraSettingsTab tab, bool enabled);

private:
    Q_DISABLE_COPY(MultipleCameraSettingsWidget)

    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;

    QnVirtualCameraResourceList m_cameras;
    bool m_hasDbChanges;

    bool m_loginWasEmpty;
    bool m_passwordWasEmpty;

    bool m_readOnly;

    bool m_updating;

    QString m_alertText;
    bool m_lockedMode = false;
};

} // namespace nx::vms::client::desktop
