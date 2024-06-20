// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resolution_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

namespace {

const std::string kResolutionStringSeparator("x");
const std::string kResolutionStringSuffix("p");
const std::string kDefaultResolutionString("-1p");
const std::regex kResolutionRegexp("^([+-]?\\d+)([px])([+-]?\\d+)?$");

} // anonymous namespace

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, ResolutionData* target)
{
    if (value.isString())
    {
        const auto str = value.toString().toStdString();
        return fromString(str, target);
    }
    return false;
}

void serialize(QnJsonContext* /*ctx*/, const ResolutionData& value, QJsonValue* target)
{
    *target = value.toString();
}

int ResolutionData::megaPixels() const
{
    if (size.isEmpty())
        return 0;

    return std::max(1, (size.width() * size.height() + 500'000) / 1'000'000);
}

std::string ResolutionData::toStdString() const
{
    if (size.isValid())
        return std::to_string(size.width()) + kResolutionStringSeparator + std::to_string(size.height());

    return std::to_string(size.height()) + kResolutionStringSuffix;
}

QString ResolutionData::toString() const
{
    return QString::fromStdString(toStdString());
}

bool fromString(const std::string_view& str, ResolutionData* target)
{
    // Regex below fails for this case.
    if (str == kDefaultResolutionString)
    {
        target->size = QSize();
        return true;
    }

    std::match_results<std::string_view::const_iterator> match;

    if (!std::regex_search(str.begin(), str.end(), match, kResolutionRegexp))
        return false;

    if (match.size() < 3)
        return false;

    try
    {
        if (match.size() < 4)
        {
            if (!(match[2] == kResolutionStringSuffix))
                return false;
            target->size.setHeight(std::stoi(match[1]));
            return true;
        }

        if (!(match[2] == kResolutionStringSeparator))
            return false;
        target->size.setWidth(std::stoi(match[1]));
        target->size.setHeight(std::stoi(match[3]));
        return true;
    }
    catch (const std::exception& e)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to deserialize ResolutionData: %1", e.what());
        return false;
    }
}

} // namespace nx::vms::api
