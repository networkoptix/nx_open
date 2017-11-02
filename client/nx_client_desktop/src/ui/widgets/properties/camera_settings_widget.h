#pragma once

#include <QtWidgets/QWidget>
#include "api/media_server_connection.h"
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include "camera_settings_tab.h"

class QStackedWidget;

class QnSingleCameraSettingsWidget;
class QnMultipleCameraSettingsWidget;

class QnCameraSettingsWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

public:
    enum Mode
    {
        InvalidMode,
        EmptyMode,
        SingleMode,
        MultiMode
    };

    QnCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnCameraSettingsWidget();

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    void setLockedMode(bool value);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

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
    void setCurrentTab(Mode mode, Qn::CameraSettingsTab tab);

private:
    QnVirtualCameraResourceList m_cameras;
    Qn::CameraSettingsTab m_emptyTab;

    QStackedWidget *m_stackedWidget;
    QWidget *m_invalidWidget;
    QWidget *m_emptyWidget;
    QnSingleCameraSettingsWidget *m_singleWidget;
    QnMultipleCameraSettingsWidget *m_multiWidget;
};
