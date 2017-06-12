#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/web_page.h>

class QnCustomCookieJar;

class CameraAdvancedSettingsWebPage: public QnWebPage
{
    using base_type = QnWebPage;
public:
    CameraAdvancedSettingsWebPage(QObject* parent = 0);

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

protected:
    //!Implementation of QWebPage::userAgentForUrl
    virtual QString	userAgentForUrl(const QUrl& url) const override;
private:
    QnCustomCookieJar* m_cookieJar;
};
