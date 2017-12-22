#pragma once

#include <nx/utils/move_only_func.h>
#include <rest/server/json_rest_handler.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace mediaserver {
namespace rest {

class Updates2RestHandler: public QnJsonRestHandler
{
public:
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
    virtual JsonRestResponse executeDelete(const JsonRestRequest& request) override;
    virtual JsonRestResponse executePost(
        const JsonRestRequest& request,
        const QByteArray& body) override;
    virtual JsonRestResponse executePut(
        const JsonRestRequest& request,
        const QByteArray& body) override;

private:
};

class UpdateRequestDataFactory
{
public:
    using FactoryFunc = utils::MoveOnlyFunc<update::info::UpdateRequestData()>;

    static update::info::UpdateRequestData create();
    static void setFactoryFunc(FactoryFunc factoryFunc);
private:
    static FactoryFunc s_factoryFunc;
};

} // namespace rest
} // namespace mediaserver
} // namespace nx
