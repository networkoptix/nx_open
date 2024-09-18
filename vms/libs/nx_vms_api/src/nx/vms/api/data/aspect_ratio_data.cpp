// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aspect_ratio_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

namespace {

const std::string kAspectRatioStringSeparator(":");
const std::string kAspectRatioStringAuto("auto");

} // anonymous namespace

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, AspectRatioData* target)
{
    if (value.isString())
    {
        const auto str = value.toString().toStdString();
        return fromString(str, target);
    }
    return false;
}

void serialize(QnJsonContext* /*ctx*/, const AspectRatioData& value, QJsonValue* target)
{
    *target = value.toString();
}

std::string AspectRatioData::toStdString() const
{
    if (size.isValid())
        return std::to_string(size.width()) + kAspectRatioStringSeparator + std::to_string(size.height());
    if (isAuto)
        return kAspectRatioStringAuto;
    return "";
}

QString AspectRatioData::toString() const
{
    return QString::fromStdString(toStdString());
}

bool fromString(const std::string_view& str, AspectRatioData* target)
{
    if (str == "")
    {
        target->size = QSize();
        target->isAuto = false;
        return true;
    }
    else if (str == kAspectRatioStringAuto)
    {
        target->size = QSize();
        target->isAuto = true;
        return true;
    }

    auto separatorPos = str.find(kAspectRatioStringSeparator);
    if (separatorPos == std::string::npos)
        return false;
    int width = 0, height = 0;
    if (std::from_chars(str.data(), str.data() + separatorPos, width).ec != std::errc()
        || std::from_chars(str.data() + separatorPos + 1, str.data() + str.size(), height).ec != std::errc())
    {
        return false;
    }

    target->size = QSize(width, height);
    target->isAuto = false;
    return true;
}

} // namespace nx::vms::api
