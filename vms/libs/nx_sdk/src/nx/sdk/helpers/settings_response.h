// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/ptr.h>
#include <nx/sdk/i_settings_response.h>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>

namespace nx {
namespace sdk {

class SettingsResponse: public RefCountable<ISettingsResponse>
{
public:
    SettingsResponse() = default;
    SettingsResponse(Ptr<StringMap> values, Ptr<StringMap> errors = nullptr);

    void setValue(std::string key, std::string value);
    void setError(std::string key, std::string value);

    void setValues(Ptr<StringMap> values);
    void setErrors(Ptr<StringMap> errors);

protected:
    virtual IStringMap* getValues() const override;
    virtual IStringMap* getErrors() const override;

private:
    Ptr<StringMap> m_values;
    Ptr<StringMap> m_errors;
};

} // namespace sdk
} // namespace nx
