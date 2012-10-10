#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>
#include "api/media_server_connection.h"
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"
#include "utils/camera_advanced_settings_xml_parser.h"
#include "ui/workbench/workbench_context_aware.h"


namespace Ui {
    class SingleCameraSettingsWidget;
}

class QVBoxLayout;

class QnCameraMotionMaskWidget;

class QnSingleCameraSettingsWidget : public QWidget, public QnWorkbenchContextAware {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

    typedef QWidget base_type;

public:
    explicit QnSingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    bool isCameraActive() const;
    void setCameraActive(bool active);

    bool hasCameraChanges() const {
        return m_hasCameraChanges;
    }
    bool hasAnyCameraChanges() const {
        return m_anyCameraChanges;
    }
    bool hasDbChanges() const {
        return m_hasDbChanges;
    }

    QnMediaServerConnectionPtr getServerConnection() const;

    const QList< QPair< QString, QVariant> >& getModifiedAdvancedParams() const {
        return m_modifiedAdvancedParamsOutgoing;
    }

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResource();
    void submitToResource();

    /** Check if motion region is valid */
    bool isValidMotionRegion();

public slots:
    void setAdvancedParam(const CameraSetting& val);
    void refreshAdvancedSettings();

signals:
    void hasChangesChanged();
    void moreLicensesRequested();
    void advancedSettingChanged();

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

private slots:
    void at_tabWidget_currentChanged();
    void at_dbDataChanged();
    void at_cameraDataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_cameraScheduleWidget_gridParamsChanged();
    void at_linkActivated(const QString &urlString);
    void at_motionTypeChanged();
    void at_motionSelectionCleared();
    void at_advancedSettingsLoaded(int httpStatusCode, const QList<QPair<QString, QVariant> >& params);
    void at_pingButtonClicked();

    void updateMaxFPS();
    void updateMotionWidgetSensitivity();

private:
    void setHasCameraChanges(bool hasChanges);
    void setAnyCameraChanges(bool hasChanges);
    void setHasDbChanges(bool hasChanges);

    void updateMotionWidgetFromResource();
    void submitMotionWidgetToResource();

    void updateMotionWidgetNeedControlMaxRect();
    void updateMotionAvailability();

    void disconnectFromMotionWidget();
    void connectToMotionWidget();

    void initAdvancedTab();
    void loadAdvancedSettings();

    void cleanAdvancedSettings();

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget)

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraSupportsMotion;

    bool m_hasCameraChanges;
    bool m_anyCameraChanges;
    bool m_hasDbChanges;

    /** Indicates that schedule was changed */
    bool m_hasScheduleChanges;

    /** Indicates that the user changed controls but not applied them to the schedule */
    bool m_hasControlsChanges;

    bool m_readOnly;

    QnCameraMotionMaskWidget *m_motionWidget;
    QVBoxLayout *m_motionLayout;
    bool m_inUpdateMaxFps;

    CameraSettings m_cameraSettings;
    CameraSettingsWidgetsTreeCreator* m_widgetsRecreator;
    QList< QPair< QString, QVariant> > m_modifiedAdvancedParams;
    QList< QPair< QString, QVariant> > m_modifiedAdvancedParamsOutgoing;
    mutable QnMediaServerConnectionPtr m_serverConnection;
};

#endif // CAMERA_SETTINGS_DIALOG_H
