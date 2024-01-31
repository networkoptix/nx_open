// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_resource.h"

#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/vms/api/data/webpage_data.h>

namespace {

const QString kSubtypePropertyName = ResourcePropertyKey::WebPage::kSubtypeKey;
const QString kProxyDomainAllowListPropertyName = "proxyDomainAllowList";
const QString kCertificateCheckPropertyName = "certificateCheck";

} // namespace

using namespace nx::vms::api;

QnWebPageResource::QnWebPageResource():
    base_type()
{
    setTypeId(nx::vms::api::WebPageData::kResourceTypeId);
    addFlags(Qn::web_page);

    connect(this, &QnResource::propertyChanged, this,
        [this](const QnResourcePtr& /*resource*/, const QString& key)
        {
            if (key == kSubtypePropertyName)
                emit subtypeChanged(toSharedPointer(this));
            else if (key == kProxyDomainAllowListPropertyName)
                emit proxyDomainAllowListChanged(toSharedPointer(this));
        });
}

QnWebPageResource::~QnWebPageResource()
{
}

void QnWebPageResource::setUrl(const QString& url)
{
    base_type::setUrl(url);
    setStatus(nx::vms::api::ResourceStatus::online);
}

QString QnWebPageResource::nameForUrl(const QUrl& url)
{
    QString name = url.host();
    if (!url.path().isEmpty())
        name += url.path();
    return name;
}

nx::vms::api::ResourceStatus QnWebPageResource::getStatus() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_status;
}

void QnWebPageResource::setStatus(nx::vms::api::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_status == newStatus)
            return;
        m_status = newStatus;
    }
    emit statusChanged(toSharedPointer(), reason);
}

WebPageSubtype QnWebPageResource::subtype() const
{
    return nx::reflect::fromString<WebPageSubtype>(getProperty(kSubtypePropertyName).toStdString());
}

void QnWebPageResource::setSubtype(WebPageSubtype value)
{
    setProperty(kSubtypePropertyName, QString::fromStdString(nx::reflect::toString(value)));
}

QStringList QnWebPageResource::proxyDomainAllowList() const
{
    QStringList allowList;

    const auto propertyData = getProperty(kProxyDomainAllowListPropertyName).toStdString();
    if (propertyData.empty()) //< Avoid deserialization error when the property is empty.
        return allowList;

    if (!nx::reflect::json::deserialize(propertyData, &allowList))
        NX_WARNING(this, "Invalid webpage domain allow list data: %1", propertyData);

    return allowList;
}

void QnWebPageResource::setProxyDomainAllowList(const QStringList& allowList)
{
    setProperty(
        kProxyDomainAllowListPropertyName,
        QString::fromStdString(nx::reflect::json::serialize(allowList)));
}

bool QnWebPageResource::isCertificateCheckEnabled() const
{
    const QString propString = getProperty(kCertificateCheckPropertyName);
    return propString.isEmpty() ? true : (propString.toUInt() > 0);
}

void QnWebPageResource::setCertificateCheckEnabled(bool value)
{
    // Default value is treated as `true`.
    setProperty(kCertificateCheckPropertyName, value ? "" : "0");
}

QnWebPageResource::Options QnWebPageResource::getOptions() const
{
    Options result;
    result.setFlag(Option::ClientApiEnabled, subtype() == nx::vms::api::WebPageSubtype::clientApi);
    result.setFlag(Option::Proxied, !getProxyId().isNull());
    return result;
}
