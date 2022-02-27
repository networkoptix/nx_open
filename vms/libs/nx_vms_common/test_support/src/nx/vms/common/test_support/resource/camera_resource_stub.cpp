// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource_stub.h"

#include <optional>

#include <core/resource/camera_user_attribute_pool.h>

#include <nx/fusion/model_functions.h>
#include <utils/crypt/symmetrical.h>

namespace nx {

struct CameraResourceStub::Private
{
    mutable QnCameraUserAttributePool attributes;
    Qn::LicenseType licenseType = Qn::LicenseType::LC_Count; //< TODO: #sivanov Replace with optional.
    std::optional<bool> hasDualStreaming;
    QMap<QnUuid, std::set<QString>> supportedObjectTypes;
};

CameraResourceStub::CameraResourceStub(Qn::LicenseType licenseType):
    CameraResourceStub({1920, 1080}, {640, 480}, licenseType)
{
}

CameraResourceStub::CameraResourceStub(
    const QSize& primaryResolution, const QSize& secondaryResolution, Qn::LicenseType licenseType)
    :
    d(new Private())
{
    d->licenseType = licenseType;
    setIdUnsafe(QnUuid::createUuid());
    setPhysicalId("physical_" + QnUuid::createUuid().toSimpleString());
    setMAC(nx::utils::MacAddress::random());
    addFlags(Qn::server_live_cam);
    setForceUseLocalProperties(true);

    CameraMediaStreams mediaStreams{{{StreamIndex::primary, primaryResolution}}};

    if (secondaryResolution.isValid())
        mediaStreams.streams.push_back({StreamIndex::secondary, secondaryResolution});

    setProperty(ResourcePropertyKey::kMediaStreams,
        QString::fromUtf8(QJson::serialized(mediaStreams)));

    setHasDualStreaming(secondaryResolution.isValid());
}

CameraResourceStub::~CameraResourceStub()
{
}

QnAbstractStreamDataProvider* CameraResourceStub::createLiveDataProvider()
{
    NX_ASSERT(false);
    return nullptr;
}

Qn::LicenseType CameraResourceStub::calculateLicenseType() const
{
    if (d->licenseType == Qn::LC_Count)
        return base_type::calculateLicenseType();

    return d->licenseType;
}

QnCameraUserAttributePool* CameraResourceStub::userAttributesPool() const
{
    return &d->attributes;
}

bool CameraResourceStub::hasDualStreamingInternal() const
{
    if (d->hasDualStreaming.has_value())
        return *d->hasDualStreaming;

    return base_type::hasDualStreamingInternal();
}

void CameraResourceStub::setHasDualStreaming(bool value)
{
    d->hasDualStreaming = value;
    setProperty(ResourcePropertyKey::kHasDualStreaming, 1); //< to reset cached values;
}

void CameraResourceStub::markCameraAsNvr()
{
    setDeviceType(nx::core::resource::DeviceType::nvr);
    d->licenseType = Qn::LC_Bridge;
    emit licenseTypeChanged(toSharedPointer(this));
}

void CameraResourceStub::setLicenseType(Qn::LicenseType licenseType)
{
    d->licenseType = licenseType;
    emit licenseTypeChanged(toSharedPointer(this));
}

bool CameraResourceStub::setProperty(
    const QString& key,
    const QString& value,
    PropertyOptions options)
{
    base_type::setProperty(key, value, options);
    emitPropertyChanged(key, QString(), value);
    return false;
}

bool CameraResourceStub::setProperty(
    const QString& key,
    const QVariant& value,
    PropertyOptions options)
{
    base_type::setProperty(key, value, options);
    emitPropertyChanged(key, QString(), value.toString());
    return false;
}

void CameraResourceStub::markCameraAsVMax()
{
    d->licenseType = Qn::LC_VMAX;
    emit licenseTypeChanged(toSharedPointer(this));
}

void CameraResourceStub::setStreamResolution(
    StreamIndex index, const QSize& resolution)
{
    if (!NX_ASSERT((index == StreamIndex::primary || index == StreamIndex::secondary)
        && resolution.isValid()))
    {
        return;
    }

    auto mediaStreams = QJson::deserialized<CameraMediaStreams>(
        getProperty(ResourcePropertyKey::kMediaStreams).toUtf8());

    const auto iter = std::find_if(mediaStreams.streams.begin(), mediaStreams.streams.end(),
        [index](const CameraMediaStreamInfo& item) { return item.getEncoderIndex() == index; });

    if (iter != mediaStreams.streams.end())
        iter->resolution = CameraMediaStreamInfo::resolutionToString(resolution);
    else
        mediaStreams.streams.push_back({index, resolution});

    setProperty(ResourcePropertyKey::kMediaStreams,
        QString::fromUtf8(QJson::serialized(mediaStreams)));
}

void CameraResourceStub::setAnalyticsObjectsEnabled(bool value, const QnUuid& engineId)
{
    if (value)
        setSupportedObjectTypes({{engineId, {"object"}}});
    else
        setSupportedObjectTypes({});
}

QSet<QnUuid> CameraResourceStub::enabledAnalyticsEngines() const
{
    return !d->supportedObjectTypes.empty()
        ? d->supportedObjectTypes.keys().toSet()
        : base_type::enabledAnalyticsEngines();
}

std::map<QnUuid, std::set<QString>> CameraResourceStub::supportedObjectTypes(
    bool filterByEngines) const
{
    return !d->supportedObjectTypes.empty()
        ? d->supportedObjectTypes.toStdMap()
        : base_type::supportedObjectTypes(filterByEngines);
}

void CameraResourceStub::setSupportedObjectTypes(
    const QMap<QnUuid, std::set<QString>>& supportedObjectTypes)
{
    d->supportedObjectTypes = supportedObjectTypes;
    emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
}

} // namespace nx
