// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk {

class SettingsResponse: public RefCountable<ISettingsResponse>
{
public:
    SettingsResponse() = default;

    SettingsResponse(
        Ptr<StringMap> values,
        Ptr<StringMap> errors = nullptr,
        Ptr<String> model = nullptr);

    void setValue(std::string key, std::string value);
    void setError(std::string key, std::string value);

    void setValues(Ptr<StringMap> values);
    void setErrors(Ptr<StringMap> errors);

    void setModel(Ptr<String> model);
    void setModel(std::string model);

protected:
    virtual IStringMap* getValues() const override;
    virtual IStringMap* getErrors() const override;
    virtual IString* getModel() const override;

private:
    Ptr<StringMap> m_values;
    Ptr<StringMap> m_errors;
    Ptr<String> m_model;
};

} // namespace nx::sdk
