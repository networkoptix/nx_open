#ifndef QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H
#define QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui {
    class CameraAdvancedSettingsWidget;
}

class QNetworkReply;
class CameraAdvancedSettingsWebPage;

class QnCameraAdvancedSettingsWidget : public Connective<QWidget> {
    Q_OBJECT

    typedef Connective<QWidget> base_type;
public:
    QnCameraAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnCameraAdvancedSettingsWidget();

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void updateFromResource();

    void reloadData();
private:
    void initWebView();

    void updatePage();

    void at_authenticationRequired(QNetworkReply* reply, QAuthenticator * authenticator);
    void at_proxyAuthenticationRequired ( const QNetworkProxy & , QAuthenticator * authenticator);
private:
    enum class Page {
        Empty,
        Manual,
        Web
    };
    void setPage(Page page);

    QScopedPointer<Ui::CameraAdvancedSettingsWidget> ui;
    Page m_page;
    QnVirtualCameraResourcePtr m_camera;
    QnMutex m_cameraMutex;
    CameraAdvancedSettingsWebPage* m_cameraAdvancedSettingsWebPage;
};

#endif // QN_CAMERA_ADVANCED_SETTINGS_WIDGET_H
