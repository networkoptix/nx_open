// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resolution_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

namespace {

const std::string kResolutionStringSeparator("x");
const std::string kResolutionStringSuffix("p");
const std::regex kResolutionRegexp("^([+-]?\\d+)([px])([+-]?\\d+)?$");

} // namespace

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, ResolutionData* target)
{
    if (value.isString())
    {
        const auto str = value.toString().toStdString();

        std::smatch match;

        if (!std::regex_search(str, match, kResolutionRegexp))
            return false;

        if (match.size() < 3)
            return false;

        try
        {
            if (match.size() == 3 || match[3].str().empty())
            {
                if (match[2] != kResolutionStringSuffix)
                    return false;

                target->size.setHeight(std::stoi(match[1].str()));
                return true;
            }

            if (match[2] != kResolutionStringSeparator)
                return false;
            target->size.setWidth(std::stoi(match[1].str()));
            target->size.setHeight(std::stoi(match[3].str()));
            return true;
        }
        catch (const std::exception& e)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to deserialize ResolutionData: %1", e.what());
            return false;
        }
    }
    return false;
}

void serialize(QnJsonContext* ctx, const ResolutionData& value, QJsonValue* target)
{
    if (value.size.isValid())
    {
        *target = QString::number(value.size.width()) + "x" + QString::number(value.size.height());
        return;
    }
    *target = QString::number(value.size.height()) + "p";
}

int ResolutionData::megaPixels() const
{
    if (size.isEmpty())
        return 0;

    return std::max(1, (size.width() * size.height() + 500'000) / 1'000'000);
}

} // namespace nx::vms::api
