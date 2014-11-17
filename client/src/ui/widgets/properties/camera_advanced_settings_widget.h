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
    void updatePage();

    void clean();
    void refresh();

    void createWidgetsRecreator(const QString &paramId);
    QnMediaServerConnectionPtr getServerConnection() const;

    void at_authenticationRequired(QNetworkReply* reply, QAuthenticator * authenticator);
    void at_proxyAuthenticationRequired ( const QNetworkProxy & , QAuthenticator * authenticator);

    void at_advancedParamChanged(const CameraSetting& val);

private slots:
    void at_advancedSettingsLoaded(int status, const QnStringVariantPairList &params, int handle);
    void at_advancedParam_saved(int httpStatusCode, const QnStringBoolPairList& operationResult);

private:
    enum class Page {
        Empty,
        CannotLoad,
        Manual,
        Web
    };

    QScopedPointer<Ui::CameraAdvancedSettingsWidget> ui;
    Page m_page;
    QnVirtualCameraResourcePtr m_camera;
    QMutex m_cameraMutex;
    CameraSettingsWidgetsTreeCreator* m_widgetsRecreator;
    CameraAdvancedSettingsWebPage* m_cameraAdvancedSettingsWebPage;
    QUrl m_lastCameraPageUrl;
    int m_paramRequestHandle;
};

#endif // QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H
