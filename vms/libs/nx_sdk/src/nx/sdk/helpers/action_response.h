// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/i_action_response.h>

#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk {

class ActionResponse: public RefCountable<IActionResponse>
{
public:
    ActionResponse() = default;

    /** Additionally, makes messageToUser() return an empty string. */
    void setActionUrl(std::string value);

    /** Additionally, makes actionUrl() return an empty string. */
    void setMessageToUser(std::string value);

    virtual const char* actionUrl() const override;
    virtual const char* messageToUser() const override;

private:
    std::string m_actionUrl;
    std::string m_messageToUser;
};

} // namespace nx::sdk
