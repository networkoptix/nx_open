#ifndef QN_CAMERA_SETTINGS_WIDGET_H
#define QN_CAMERA_SETTINGS_WIDGET_H

#include <QWidget>
#include "api/media_server_connection.h"
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include "camera_settings_tab.h"

class QStackedWidget;

class QnSingleCameraSettingsWidget;
class QnMultipleCameraSettingsWidget;

class QnCameraSettingsWidget: public QWidget, public QnWorkbenchContextAware {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

public:
    enum Mode {
        InvalidMode,
        EmptyMode,
        SingleMode,
        MultiMode
    };

    QnCameraSettingsWidget(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);
    virtual ~QnCameraSettingsWidget();

    const QnResourceList &resources() const;
    const QnVirtualCameraResourceList &cameras() const;
    void setResources(const QnResourceList &resources);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    int activeCameraCount() const;
    void setCamerasActive(bool active);

    bool hasDbChanges() const;
    bool hasCameraChanges() const;
    bool hasAnyCameraChanges() const;

    /** Checks if user changed controls but not applied them to the schedule */
    bool hasControlsChanges() const;

    const QList< QPair< QString, QVariant> >& getModifiedAdvancedParams() const;
    QnMediaServerConnectionPtr getServerConnection() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly) const;

    void updateFromResources();
    void submitToResources();

    Mode mode() const;

    /** Check if motion region is valid */
    bool isValidMotionRegion();
signals:
    void hasChangesChanged();
    void modeChanged();
    void moreLicensesRequested();
    void advancedSettingChanged();

protected slots:
    void at_moreLicensesRequested();
    void at_advancedSettingChanged();

private:
    void setMode(Mode mode);
    void setCurrentTab(Mode mode, Qn::CameraSettingsTab tab);

private:
    QnResourceList m_resources;
    QnVirtualCameraResourceList m_cameras;
    Qn::CameraSettingsTab m_emptyTab;

    QStackedWidget *m_stackedWidget;
    QWidget *m_invalidWidget;
    QWidget *m_emptyWidget;
    QnSingleCameraSettingsWidget *m_singleWidget;
    QnMultipleCameraSettingsWidget *m_multiWidget;
};


#endif // QN_CAMERA_SETTINGS_WIDGET_H
