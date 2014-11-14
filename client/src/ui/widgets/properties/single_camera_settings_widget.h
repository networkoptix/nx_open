#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtWidgets/QWidget>
#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QtWebKitWidgets>

#include "utils/camera_advanced_settings_xml_parser.h"
#include "utils/common/connective.h"

#include <core/resource/resource_fwd.h>

#include "api/media_server_connection.h"

#include "ui/workbench/workbench_context_aware.h"

#include "camera_settings_tab.h"

namespace Ui {
    class SingleCameraSettingsWidget;
}

class QVBoxLayout;
class QnCameraMotionMaskWidget;
class QnCameraSettingsWidgetPrivate;
class QnImageProvider;
class CameraAdvancedSettingsWebPage;

class QnSingleCameraSettingsWidget : public Connective<QWidget>, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef Connective<QWidget> base_type;

public:
    explicit QnSingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const;

    bool hasDbChanges() const {
        return m_hasDbChanges;
    }

    /** Checks if user changed schedule controls but not applied them */
    bool hasScheduleControlsChanges() const {
        return m_hasScheduleControlsChanges;
    }

    /** Clear flag that user changed schedule controls but not applied them */
    void clearScheduleControlsChanges() {
        m_hasScheduleControlsChanges = false;
    }

    /** Checks if user changed motion controls but not applied them */
    bool hasMotionControlsChanges() const {
        return m_hasMotionControlsChanges;
    }

    /** Clear flag that  user changed motion controls but not applied them */
    void clearMotionControlsChanges() {
        m_hasMotionControlsChanges = false;
    }

    QnMediaServerConnectionPtr getServerConnection() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    //!Return true, if some parameter(s), requiring license validation has(-ve) been changed
    bool licensedParametersModified() const;
    void updateFromResource();
    void reject();
    void submitToResource();

    /** Check if motion region is valid */
    bool isValidMotionRegion();

    /** Check if second stream is enabled if there are Motion+LQ tasks in schedule. */
    bool isValidSecondStream();

    void setExportScheduleButtonEnabled(bool enabled);

public slots:
    void at_advancedParamChanged(const CameraSetting& val);
    void refreshAdvancedSettings();

signals:
    void hasChangesChanged();
    void moreLicensesRequested();
    void scheduleExported(const QnVirtualCameraResourceList &);

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

    QnCameraSettingsWidgetPrivate* d_ptr;

private slots:
    void at_tabWidget_currentChanged();
    void at_dbDataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_cameraScheduleWidget_recordingSettingsChanged();
    void at_cameraScheduleWidget_gridParamsChanged();
    void at_cameraScheduleWidget_controlsChangesApplied();
    void at_cameraScheduleWidget_scheduleEnabledChanged();
    void at_linkActivated(const QString &urlString);
    void at_motionTypeChanged();
    void at_resetMotionRegionsButton_clicked();
    void at_motionRegionListChanged();
    void at_advancedSettingsLoaded(int status, const QnStringVariantPairList &params, int handle);
    void at_advancedParam_saved(int httpStatusCode, const QnStringBoolPairList& operationResult);
    void at_analogViewCheckBox_clicked();
    void at_fisheyeSettingsChanged();

    void updateMaxFPS();
    void updateMotionWidgetSensitivity();
    void updateLicensesButtonVisible();
    void updateLicenseText();
    void updateIpAddressText();
    void updateWebPageText();

private:
    void setHasDbChanges(bool hasChanges);

    void updateMotionWidgetFromResource();
    void submitMotionWidgetToResource();

    void updateMotionWidgetNeedControlMaxRect();
    void updateMotionAvailability();
    void updateRecordingParamsAvailability();

    void disconnectFromMotionWidget();
    void connectToMotionWidget();

    bool initAdvancedTab();
    void loadAdvancedSettings();

    void cleanAdvancedSettings();
    void updateWebPage(QStackedLayout* stackedLayout , QWebView* advancedWebView);

    Q_SLOT void at_authenticationRequired(QNetworkReply* reply, QAuthenticator * authenticator);
    Q_SLOT void at_proxyAuthenticationRequired ( const QNetworkProxy & , QAuthenticator * authenticator);

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget)
    Q_DECLARE_PRIVATE(QnCameraSettingsWidget)

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QMutex m_cameraMutex;
    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraSupportsMotion;

    bool m_hasDbChanges;

    bool m_scheduleEnabledChanged;
    /** Indicates that schedule was changed */
    bool m_hasScheduleChanges;

    /** Indicates that the user changed schedule controls but not applied them */
    bool m_hasScheduleControlsChanges;

    /** Indicates that the user changed motion sensitivity controls but not applied them */
    bool m_hasMotionControlsChanges;

    bool m_readOnly;

    bool m_updating;

    QnCameraMotionMaskWidget *m_motionWidget;
    QVBoxLayout *m_motionLayout;
    bool m_inUpdateMaxFps;

    CameraSettings m_cameraSettings;
    CameraSettingsWidgetsTreeCreator* m_widgetsRecreator;
    mutable QnMediaServerConnectionPtr m_serverConnection;

    QHash<QnUuid, QnImageProvider*> m_imageProvidersByResourceId;
	QUrl m_lastCameraPageUrl;
    CameraAdvancedSettingsWebPage* m_cameraAdvancedSettingsWebPage;
};

#endif // CAMERA_SETTINGS_DIALOG_H
