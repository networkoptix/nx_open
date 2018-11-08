#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>

class QLabel;
class QStackedWidget;

namespace nx::vms::client::desktop {

class SingleCameraSettingsWidget;
class MultipleCameraSettingsWidget;

class CameraSettingsWidget: public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    enum Mode
    {
        InvalidMode,
        EmptyMode,
        SingleMode,
        MultiMode
    };

    CameraSettingsWidget(QWidget* parent = nullptr);
    virtual ~CameraSettingsWidget();

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    void setLockedMode(bool value);

    CameraSettingsTab currentTab() const;
    void setCurrentTab(CameraSettingsTab tab);

    bool hasDbChanges() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly) const;

    void updateFromResources();
    void submitToResources();
    void reject();

    Mode mode() const;

    /** Check if motion region is valid */
    bool isValidMotionRegion();

    /** Check if second stream is enabled if there are Motion+LQ tasks in schedule. */
    bool isValidSecondStream();

    void setExportScheduleButtonEnabled(bool enabled);

signals:
    void hasChangesChanged();
    void modeChanged();
    void resourcesChanged();

private:
    void setMode(Mode mode);
    void setCurrentTab(Mode mode, CameraSettingsTab tab);

    void updateInvalidText();

private:
    QnVirtualCameraResourceList m_cameras;
    CameraSettingsTab m_emptyTab;

    QStackedWidget *m_stackedWidget;
    QLabel *m_invalidWidget;
    QLabel *m_emptyWidget;
    SingleCameraSettingsWidget *m_singleWidget;
    MultipleCameraSettingsWidget *m_multiWidget;
};

} // namespace nx::vms::client::desktop
