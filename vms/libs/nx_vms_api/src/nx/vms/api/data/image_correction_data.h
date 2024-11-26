// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/compare.h>
#include <nx/reflect/instrument.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API ImageCorrectionData
{
    static constexpr qreal kDefaultBlackLevel = 0.001;
    static constexpr qreal kDefaultWhiteLevel = 0.0005;
    static constexpr qreal kDefaultGamma = 1.0;

    /**%apidoc Whether the image enhancement is enabled.*/
    bool enabled = false;
    /**%apidoc [opt] Level of the black color (floating-point, 0..99).*/
    qreal blackLevel = kDefaultBlackLevel;
    /**%apidoc [opt] Level of the white color (floating-point, 0..99).*/
    qreal whiteLevel = kDefaultWhiteLevel;
    /**%apidoc [opt] Gamma enhancement value (floating-point, 0.1..10).*/
    qreal gamma = kDefaultGamma;

    bool operator==(const ImageCorrectionData& other) const;

    /**
     * Compatibility-layer function to maintain old way of (de)serializing. Used in the server sql
     * database and in the pre-2.3 client exported layouts.
     */
    QByteArray toByteArray() const;

    /**
     * Compatibility-layer function to maintain old way of (de)serializing. Used in the server sql
     * database and in the pre-2.3 client exported layouts.
     */
    static ImageCorrectionData fromByteArray(const QByteArray& data);
};
#define ImageCorrectionData_Fields (enabled)(blackLevel)(whiteLevel)(gamma)
NX_VMS_API_DECLARE_STRUCT_EX(ImageCorrectionData, (ubjson)(json)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT(ImageCorrectionData, ImageCorrectionData_Fields)

} // namespace nx::vms::api

// Compatibility-layer functions to maintain old way of (de)serializing in the server sql database.
void NX_VMS_API serialize_field(const nx::vms::api::ImageCorrectionData& data, QVariant* target);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::ImageCorrectionData* target);
