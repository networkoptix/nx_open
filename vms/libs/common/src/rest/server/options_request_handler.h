#pragma once

#include "request_handler.h"

class OptionsRequestHandler:
    public QnRestRequestHandler
{
public:
    virtual RestResponse executeRequest(
        nx::network::http::Method::ValueType method,
        const RestRequest& request,
        const nx::String& requestContentType,
        const QByteArray& requestBody);
};
