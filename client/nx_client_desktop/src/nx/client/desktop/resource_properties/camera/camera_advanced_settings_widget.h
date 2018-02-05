#pragma once

#include <QtWidgets/QWidget>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui {
class CameraAdvancedSettingsWidget;
}

class QNetworkReply;
class QNetworkProxy;
class QAuthenticator;

namespace nx {
namespace client {
namespace desktop {

class CameraAdvancedSettingsWebPage;

class CameraAdvancedSettingsWidget: public Connective<QWidget>, public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = Connective<QWidget>;
public:
    CameraAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~CameraAdvancedSettingsWidget();

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void updateFromResource();

    void reloadData();

    bool hasChanges() const;

    void submitToResource();

    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void hasChangesChanged();

protected:
    virtual void hideEvent(QHideEvent* event) override;

private:
    void initWebView();

    void updatePage();
    void updateUrls();

    void at_authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
    void at_proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator);

    bool hasManualPage() const;
    bool hasWebPage() const;
private:
    enum Page
    {
        Empty,
        Manual,
        Web,
    };
    //void setPage(Page page);

    QScopedPointer<Ui::CameraAdvancedSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    QnMutex m_cameraMutex;
    CameraAdvancedSettingsWebPage* m_cameraAdvancedSettingsWebPage;
};

} // namespace desktop
} // namespace client
} // namespace nx
