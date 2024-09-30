// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

/**%apidoc:string
 * Resolution in format `{width}x{height}`. The string either may contain width
 * and height (for instance 320x240) or height only (for instance 240p).
 * By default, 640x480 is used.
 */
struct NX_VMS_API ResolutionData
{
    template<typename... Args>
    ResolutionData(Args&&... args): size(std::forward<Args>(args)...) {}

    operator QSize() const { return size; }
    operator QSize&() { return size; }

    int megaPixels() const;

    std::string toStdString() const;
    QString toString() const;

    /**%apidoc[unused] */
    QSize size;
};

bool NX_VMS_API deserialize(QnJsonContext* ctx, const QJsonValue& value, ResolutionData* target);
void NX_VMS_API serialize(QnJsonContext* ctx, const ResolutionData& value, QJsonValue* target);

bool NX_VMS_API fromString(const std::string_view& str, ResolutionData* target);

NX_REFLECTION_TAG_TYPE(ResolutionData, useStringConversionForSerialization)

} // namespace nx::vms::api
