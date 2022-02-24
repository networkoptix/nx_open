// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dewarping_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

namespace nx::vms::api::dewarping {

namespace {

static const int kPanoLow = 1;
static const int kPanoNormal = 2;
static const int kPanoWide = 4;

static const QList<int> kHorizontalPano({kPanoLow, kPanoNormal});
static const QList<int> kVerticalPano({kPanoLow, kPanoNormal, kPanoWide});
static const QList<int> k360Pano({kPanoLow, kPanoNormal, kPanoWide});

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MediaData,
    (ubjson)(json),
    MediaData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ViewData,
    (ubjson)(json)(xml)(csv_record),
    ViewData_Fields)

const QList<int>& MediaData::allowedPanoFactorValues() const
{
    return is360VR() ? k360Pano : allowedPanoFactorValues(viewMode);
}

const QList<int>& MediaData::allowedPanoFactorValues(FisheyeCameraMount mode)
{
    return (mode == FisheyeCameraMount::wall)
        ? kHorizontalPano
        : kVerticalPano;
}

bool MediaData::is360VR() const
{
    return is360VR(cameraProjection);
}

bool MediaData::is360VR(CameraProjection projection)
{
    return projection == CameraProjection::equirectangular360;
}

bool MediaData::isFisheye() const
{
    return isFisheye(cameraProjection);
}

bool MediaData::isFisheye(CameraProjection projection)
{
    return !is360VR(projection);
}

QByteArray MediaData::toByteArray() const
{
    return QJson::serialized(*this);
}

MediaData MediaData::fromByteArray(const QByteArray& data)
{
    if (data.startsWith('0') || data.startsWith('1'))
    {
        // Old format.
        MediaData result;
        auto params = data.split(';').toVector();
        params.resize(6);
        result.enabled = params[0].toInt() > 0;
        result.viewMode = FisheyeCameraMount(params[1].toInt());
        result.fovRot = params[5].toDouble();
        return result;
    }

    return QJson::deserialized<MediaData>(data);
}

bool MediaData::operator==(const MediaData& other) const
{
    return nx::reflect::equals(*this, other);
}

//-------------------------------------------------------------------------------------------------
// ViewData

QByteArray ViewData::toByteArray() const
{
    return QJson::serialized(*this);
}

ViewData ViewData::fromByteArray(const QByteArray& data)
{
    return QJson::deserialized<ViewData>(data);
}

bool ViewData::operator==(const ViewData& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::api::dewarping

void serialize_field(const nx::vms::api::dewarping::MediaData& data, QVariant* target)
{
    serialize_field(data.toByteArray(), target);
}

void deserialize_field(const QVariant& value, nx::vms::api::dewarping::MediaData* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    *target = nx::vms::api::dewarping::MediaData::fromByteArray(tmp);
}

void serialize_field(const nx::vms::api::dewarping::ViewData& data, QVariant* target)
{
    serialize_field(data.toByteArray(), target);
}

void deserialize_field(const QVariant& value, nx::vms::api::dewarping::ViewData* target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    *target = nx::vms::api::dewarping::ViewData::fromByteArray(tmp);
}
