#pragma once

#include "request_handler.h"

class OptionsRequestHandler:
    public QnRestRequestHandler
{
public:
    virtual RestResponse executeRequest(const RestRequest& request) override;
};
