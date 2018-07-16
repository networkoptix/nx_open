#pragma once

#include <rest/server/json_rest_handler.h>

namespace nx {

class UpdateInformationRestHandler: public QnJsonRestHandler
{
public:
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;

private:
};

} // namespace nx
