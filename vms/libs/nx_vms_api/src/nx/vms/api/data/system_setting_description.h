// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qjson.h>

namespace nx::vms::api {

struct NX_VMS_API SystemSettingDescription
{
    QString label;

    /**%apidoc:enum
     * %value Null
     * %value Bool
     * %value Double
     * %value String
     * %value Array
     * %value Object
     */
    QJsonValue::Type type;

    bool isReadOnly = false;
    bool isWriteOnly = false;

    /**
     * Can only be modified by Administrators by default. Power Users require `securityForPowerUsers`
     * Setting to be enabled in order to modify these.
     */
    bool isSecurity = false;

    /** Can only be modified by Administrators. This value depends on `securityForPowerUsers`. */
    bool isOwnerOnly = false;
};
#define SystemSettingDescription_Fields (label)(type)(isReadOnly)(isWriteOnly)(isSecurity)(isOwnerOnly)
QN_FUSION_DECLARE_FUNCTIONS(SystemSettingDescription, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(SystemSettingDescription, SystemSettingDescription_Fields)

using SystemSettingDescriptions = std::map<QString, SystemSettingDescription>;

// TODO: Implement & use ValueOrMap instead.
// And it _must_ be a class for some reason: with struct apidoc reports an error.
class SystemSettingsValues: public std::map<QString, QJsonValue>
{
public:
    [[nodiscard]] const auto& getId() const
    {
        static const key_type defaultValue{};
        const key_type& result = 1 == size() ? begin()->first : defaultValue;

        // decltype(auto) will be deduced as const reference here
        return result;
    }
};

} // namespace nx::vms::api
