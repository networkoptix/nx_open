#pragma once

#include <QtCore/QMetaType>

#include "data.h"

namespace nx::vms::api {

struct NX_VMS_API ImageCorrectionData: Data
{
    bool enabled = false;
    qreal blackLevel = 0.001;
    qreal whiteLevel = 0.0005;
    qreal gamma = 1.0;

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

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::ImageCorrectionData)

//Compatibility-layer functions to maintain old way of (de)serializing in the server sql database.
void NX_VMS_API serialize_field(const nx::vms::api::ImageCorrectionData& data, QVariant* target);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::ImageCorrectionData* target);
