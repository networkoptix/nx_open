// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_response.h"

namespace nx::sdk {

void ActionResponse::setActionUrl(std::string value)
{
    m_actionUrl = std::move(value);
    m_messageToUser = "";
}

void ActionResponse::setMessageToUser(std::string value)
{
    m_messageToUser = std::move(value);
    m_actionUrl = "";
}

const char* ActionResponse::actionUrl() const
{
    return m_actionUrl.c_str();
}

const char* ActionResponse::messageToUser() const
{
    return m_messageToUser.c_str();
}

} // namespace nx::sdk
