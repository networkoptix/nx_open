#ifndef QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H
#define QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui {
    class CameraAdvancedSettingsWidget;
}

class QNetworkReply;
class CameraSettingsWidgetsTreeCreator;
class CameraAdvancedSettingsWebPage;
class CameraSetting;
class QnCameraAdvancedParamsReader;
class QnCameraAdvancedParamWidgetsManager;

class QnCameraAdvancedSettingsWidget : public Connective<QWidget> {
    Q_OBJECT

    typedef Connective<QWidget> base_type;
public:
    QnCameraAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnCameraAdvancedSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void updateFromResource();

    void reloadData();
private:
    void initWebView();

    void updatePage();

    QnMediaServerConnectionPtr getServerConnection() const;

    void at_authenticationRequired(QNetworkReply* reply, QAuthenticator * authenticator);
    void at_proxyAuthenticationRequired ( const QNetworkProxy & , QAuthenticator * authenticator);

    void at_advancedParamChanged(const QString &id, const QString &value);

    void updateApplyingParamsLabel();

private slots:
    void at_advancedSettingsLoaded(int status, const QnCameraAdvancedParamValueList &params, int handle);
    void at_advancedParam_saved(int status, const QnCameraAdvancedParamValueList &params, int handle);

private:
    enum class Page {
        Empty,
        CannotLoad,
        Manual,
        Web
    };
    void setPage(Page page);

    QScopedPointer<Ui::CameraAdvancedSettingsWidget> ui;
	QScopedPointer<QnCameraAdvancedParamsReader> m_advancedParamsReader;
	QScopedPointer<QnCameraAdvancedParamWidgetsManager> m_advancedParamWidgetsManager;
    Page m_page;
    QnVirtualCameraResourcePtr m_camera;
    QMutex m_cameraMutex;
    CameraAdvancedSettingsWebPage* m_cameraAdvancedSettingsWebPage;
    QUrl m_lastCameraPageUrl;
    int m_paramRequestHandle;
    int m_applyingParamsCount;
};

#endif // QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H
