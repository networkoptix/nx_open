// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource_stub.h"

#include <optional>

#include <core/resource/camera_media_stream_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/qt_helpers.h>

namespace nx {

struct CameraResourceStub::Private
{
    Qn::LicenseType licenseType = Qn::LicenseType::LC_Count; //< TODO: #sivanov Replace with optional.
    std::optional<bool> hasDualStreaming;

    QMap<nx::Uuid, std::set<QString>> supportedObjectTypes;
    QMap<nx::Uuid, std::set<QString>> supportedEventTypes;

    std::optional<UuidSet> enabledAnalyticsEngines;
    std::optional<nx::vms::common::AnalyticsEngineResourceList> compatibleAnalyticsEngineResources;
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
    setIdUnsafe(nx::Uuid::createUuid());
    setPhysicalId("physical_" + nx::Uuid::createUuid().toSimpleString());
    setMAC(nx::utils::MacAddress::random());
    addFlags(Qn::server_live_cam);
    setForceUsingLocalProperties();

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

bool CameraResourceStub::hasDualStreamingInternal() const
{
    if (d->hasDualStreaming.has_value())
        return *d->hasDualStreaming;

    return base_type::hasDualStreamingInternal();
}

void CameraResourceStub::setHasDualStreaming(bool value)
{
    d->hasDualStreaming = value;
    setProperty(ResourcePropertyKey::kHasDualStreaming, "1"); //< Reset cached values.
}

void CameraResourceStub::markCameraAsNvr()
{
    setDeviceType(nx::vms::api::DeviceType::nvr);
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
    bool markDirty)
{
    base_type::setProperty(key, value, markDirty);
    emitPropertyChanged(key, QString(), value);
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

void CameraResourceStub::setAnalyticsObjectsEnabled(bool value, const nx::Uuid& engineId)
{
    if (value)
        setSupportedObjectTypes({{engineId, {"object"}}});
    else
        setSupportedObjectTypes({});
}

QSet<nx::Uuid> CameraResourceStub::enabledAnalyticsEngines() const
{
    if (d->enabledAnalyticsEngines)
        return *d->enabledAnalyticsEngines;

    return !d->supportedObjectTypes.empty() || !d->supportedEventTypes.empty()
        ? nx::utils::toQSet(d->supportedObjectTypes.keys())
              .unite(nx::utils::toQSet(d->supportedEventTypes.keys()))
        : base_type::enabledAnalyticsEngines();
}

nx::vms::common::AnalyticsEngineResourceList
    CameraResourceStub::compatibleAnalyticsEngineResources() const
{
    if (d->compatibleAnalyticsEngineResources)
        return *d->compatibleAnalyticsEngineResources;

    return base_type::compatibleAnalyticsEngineResources();
}

std::map<nx::Uuid, std::set<QString>> CameraResourceStub::supportedObjectTypes(
    bool filterByEngines) const
{
    return !d->supportedObjectTypes.empty()
        ? d->supportedObjectTypes.toStdMap()
        : base_type::supportedObjectTypes(filterByEngines);
}

std::map<nx::Uuid, std::set<QString>> CameraResourceStub::supportedEventTypes() const
{
    return !d->supportedEventTypes.empty()
        ? d->supportedEventTypes.toStdMap()
        : base_type::supportedEventTypes();
}

void CameraResourceStub::setSupportedObjectTypes(
    const QMap<nx::Uuid, std::set<QString>>& supportedObjectTypes)
{
    d->supportedObjectTypes = supportedObjectTypes;
    emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
}

void CameraResourceStub::setSupportedEventTypes(
    const QMap<nx::Uuid, std::set<QString>>& supportedObjectTypes)
{
    d->supportedEventTypes = supportedObjectTypes;
    emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
}

void CameraResourceStub::setEnabledAnalyticsEngines(UuidSet engines)
{
    d->enabledAnalyticsEngines = std::move(engines);
    emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
    emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
}

void CameraResourceStub::setCompatibleAnalyticsEngineResources(
    nx::vms::common::AnalyticsEngineResourceList engines)
{
    d->compatibleAnalyticsEngineResources = std::move(engines);
    emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
    emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
}

} // namespace nx
