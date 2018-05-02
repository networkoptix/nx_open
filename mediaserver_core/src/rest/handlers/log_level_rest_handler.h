#pragma once

#include <rest/server/json_rest_handler.h>

class QnLogLevelRestHandler: public QnJsonRestHandler
{
public:
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;

private:
    virtual JsonRestResponse manageLogLevelById(const JsonRestRequest& request);
};
