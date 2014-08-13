#ifndef QN_CAMERA_SETTINGS_WIDGET_H
#define QN_CAMERA_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
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

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const;

    bool hasDbChanges() const;
    bool hasCameraChanges() const;
    bool hasAnyCameraChanges() const;

    /** Checks if user changed schedule controls but not applied them */
    bool hasScheduleControlsChanges() const;

    /** Clear flag that user changed schedule controls but not applied them */
    void clearScheduleControlsChanges();

    /** Checks if user changed motion controls but not applied them */
    bool hasMotionControlsChanges() const;

    /** Clear flag that  user changed motion controls but not applied them */
    void clearMotionControlsChanges();

    const QList< QPair< QString, QVariant> >& getModifiedAdvancedParams() const;
    QnMediaServerConnectionPtr getServerConnection() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly) const;

    //!Return true, if some parameter(s), requiring license validation has(-ve) been changed
    bool licensedParametersModified() const;

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
    void moreLicensesRequested();
    void advancedSettingChanged();
    void scheduleExported(const QnVirtualCameraResourceList &);
    void resourcesChanged();

protected slots:
    void at_moreLicensesRequested();
    void at_advancedSettingChanged();

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


#endif // QN_CAMERA_SETTINGS_WIDGET_H
