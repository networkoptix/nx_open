#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/web_page.h>

namespace nx {
namespace client {
namespace desktop {

class CameraAdvancedSettingsWebPage: public QnWebPage
{
    using base_type = QnWebPage;
public:
    CameraAdvancedSettingsWebPage(QObject* parent = 0);

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

protected:
    //!Implementation of QWebPage::userAgentForUrl
    virtual QString userAgentForUrl(const QUrl& url) const override;

private:
    class CustomCookieJar;

    CustomCookieJar* m_cookieJar;
};

} // namespace desktop
} // namespace client
} // namespace nx
