#pragma once

#include <rest/server/json_rest_handler.h>

class QnLogLevelRestHandler: public QnJsonRestHandler
{
public:
    RestResponse executeGet(const RestRequest& request) override;

private:
    virtual RestResponse manageLogLevelById(const RestRequest& request);
};
