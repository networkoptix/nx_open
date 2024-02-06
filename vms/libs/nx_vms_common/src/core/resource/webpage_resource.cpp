// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_resource.h"

#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/serialization/qt_geometry_reflect_json.h>
#include <nx/vms/api/data/webpage_data.h>

namespace {

const QString kSubtypePropertyName = ResourcePropertyKey::WebPage::kSubtypeKey;
const QString kProxyDomainAllowListPropertyName = "proxyDomainAllowList";
const QString kCertificateCheckPropertyName = "certificateCheck";
static const QString kRefreshIntervalPropertyName = "refreshIntervalS";
static const QString kOpenInDialogPropertyName = "openInDialog";
static const QString kDialogSizePropertyName = "dialogSize";

} // namespace

using namespace nx::vms::api;
using namespace std::chrono;

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
            else if (key == kRefreshIntervalPropertyName)
                emit refreshIntervalChanged();
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

seconds QnWebPageResource::refreshInterval() const
{
    const QString propString = getProperty(kRefreshIntervalPropertyName);
    return propString.isEmpty() ? 0s : seconds(propString.toUInt());
}

void QnWebPageResource::setRefreshInterval(seconds interval)
{
    setProperty(kRefreshIntervalPropertyName, QString::number(interval.count()));
}

bool QnWebPageResource::isOpenInDialog() const
{
    auto [value, _] =
        nx::reflect::json::deserialize<bool>(getProperty(kOpenInDialogPropertyName).toStdString());

    return value;
}

void QnWebPageResource::setOpenInDialog(bool value)
{
    setProperty(
        kOpenInDialogPropertyName, QString::fromStdString(nx::reflect::json::serialize(value)));
}

QSize QnWebPageResource::dialogSize() const
{
    auto [size, _] =
        nx::reflect::json::deserialize<QSize>(getProperty(kDialogSizePropertyName).toStdString());

    return size;
}

void QnWebPageResource::setDialogSize(const QSize& size)
{
    setProperty(
        kDialogSizePropertyName, QString::fromStdString(nx::reflect::json::serialize(size)));
}

QnWebPageResource::Options QnWebPageResource::getOptions() const
{
    Options result;
    result.setFlag(Option::ClientApiEnabled, subtype() == nx::vms::api::WebPageSubtype::clientApi);
    result.setFlag(Option::Proxied, !getProxyId().isNull());
    return result;
}
