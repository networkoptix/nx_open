#include "client.h"

#include "request_path.h"

namespace nx::network::maintenance::log {

Client::Client(const nx::utils::Url& baseApiUrl):
    base_type(baseApiUrl)
{
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::getConfiguration(
    nx::utils::MoveOnlyFunc<void(ResultCode, Loggers)> completionHandler)
{
    base_type::template makeAsyncCall<Loggers>(
        kLoggers,
        std::move(completionHandler));
}

std::tuple<Client::ResultCode, Loggers> Client::getConfiguration()
{
    return base_type::template makeSyncCall<Loggers>(kLoggers);
}

} // namespace nx::network::maintenance::log
