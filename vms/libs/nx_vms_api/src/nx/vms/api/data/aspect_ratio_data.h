// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

/**%apidoc:string
 * Aspect ratio in format `{width}:{height}`. The string either may contain width and height
 * (for instance 320:240), or keyword 'auto' for keeping aspect ratio from device settings.
 */
struct NX_VMS_API AspectRatioData
{
    template<typename... Args>
    AspectRatioData(Args&&... args): size(std::forward<Args>(args)...) {}
    AspectRatioData(bool isAuto = false): isAuto(isAuto) {}

    operator QSize() const { return size; }
    operator QSize&() { return size; }

    std::string toStdString() const;
    QString toString() const;

    /**%apidoc[unused] */
    bool isAuto = false;
    /**%apidoc[unused] */
    QSize size;
};

bool NX_VMS_API deserialize(QnJsonContext* ctx, const QJsonValue& value, AspectRatioData* target);
void NX_VMS_API serialize(QnJsonContext* ctx, const AspectRatioData& value, QJsonValue* target);

bool NX_VMS_API fromString(const std::string_view& str, AspectRatioData* target);

NX_REFLECTION_TAG_TYPE(AspectRatioData, useStringConversionForSerialization)

} // namespace nx::vms::api
