// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource.h>

// TODO: #sivanov Move to the corresponding library.
namespace nx {
namespace vms {
namespace api {

enum class WebPageSubtype
{
    /**%apidoc[unused] */
    none,
    clientApi //< Our additional JavaScript API exported to webpage.
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(WebPageSubtype*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<WebPageSubtype>;
    return visitor(
        Item{WebPageSubtype::none, ""},
        Item{WebPageSubtype::clientApi, "clientApi"}
    );
}

} // namespace api
} // namespace vms
} // namespace nx

class NX_VMS_COMMON_API QnWebPageResource: public QnResource
{
    Q_OBJECT
    using base_type = QnResource;

public:
    enum Option
    {
        ClientApiEnabled = 1 << 0,
        Proxied = 1 << 1,

        WebPage = 0,
        Integration = ClientApiEnabled,
        ProxiedWebPage = Proxied,
        ProxiedIntegration = ClientApiEnabled | Proxied
    };
    Q_DECLARE_FLAGS(Options, Option);

public:
    QnWebPageResource();

    virtual ~QnWebPageResource();

    virtual void setUrl(const QString& url) override;

    static QString nameForUrl(const QUrl& url);

    virtual nx::vms::api::ResourceStatus getStatus() const override;
    virtual void setStatus(nx::vms::api::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    /** Check if the page is the special integration page. */
    nx::vms::api::WebPageSubtype subtype() const;
    void setSubtype(nx::vms::api::WebPageSubtype value);

    /** List of domains (with wildcards) allowed to be proxied for this web page. */
    QStringList proxyDomainAllowList() const;
    void setProxyDomainAllowList(const QStringList& allowList);

    /** Get proxy server id, null id means no proxy. */
    QnUuid getProxyId() const { return getParentId(); }
    /** Set server id through which the page should be proxied, null id means no proxy. */
    void setProxyId(const QnUuid& proxyServerId) { setParentId(proxyServerId); }

    /** SSL certificate check flag. Default is true. */
    bool isCertificateCheckEnabled() const;
    void setCertificateCheckEnabled(bool value);

    Options getOptions() const;

signals:
    void subtypeChanged(const QnWebPageResourcePtr& webPage);
    void proxyDomainAllowListChanged(const QnWebPageResourcePtr& webPage);

private:
    // This class must not store its status on server side
    nx::vms::api::ResourceStatus m_status = nx::vms::api::ResourceStatus::undefined;
};
