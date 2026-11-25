// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini_cloud_feature_source.h"

#include <nx/reflect/enum_string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std_string_utils.h>
#include <nx/vms/client/core/ini.h>

namespace nx::vms::client::core {

namespace {

CloudFeatures parseFeatures(std::string_view tokens)
{
    CloudFeatures result;
    nx::utils::split(
        tokens,
        nx::utils::separator::isAnyOf(" ,;"),
        [&result](std::string_view token)
        {
            const auto feature = nx::reflect::enumeration::fromStringStrict<CloudFeature>(token);
            if (feature.has_value())
                result.setFlag(feature.value());
            else
                NX_WARNING(NX_SCOPE_TAG, "Unknown cloud feature %1 in the INI file", token);
        },
        nx::utils::GroupToken::none,
        nx::utils::SplitterFlag::skipEmpty);
    return result;
}

} // namespace

struct IniCloudFeatureSource::Private
{
    const CloudFeatures disabledFeatures;
};

IniCloudFeatureSource::IniCloudFeatureSource(QObject* parent):
    IniCloudFeatureSource(ini().forcefullyDisabledFeatures, parent)
{
}

IniCloudFeatureSource::IniCloudFeatureSource(std::string_view disabledFeatures, QObject* parent):
    AbstractCloudFeatureSource(parent),
    d(new Private{.disabledFeatures = parseFeatures(disabledFeatures)})
{
}

IniCloudFeatureSource::~IniCloudFeatureSource()
{
}

CloudFeatures IniCloudFeatureSource::enabledFeatures() const
{
    return allCloudFeatures() & ~d->disabledFeatures;
}

} // namespace nx::vms::client::core
