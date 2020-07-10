#include "client.h"

#include <memory>
#include <system_error>

#include <nx/utils/general_macros.h>
#include "../buffer_source.h"

namespace nx::network::http::futures {

using namespace nx::utils;

namespace {

template <typename Func, typename... Args>
cf::future<Response> futurize(Func func, Client* client, Args&&... args)
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();

    func(*client, std::forward<Args>(args)...,
        [promise = std::move(promise)]() mutable { promise.set_value(cf::unit()); });

    return future
        .then(cf::translate_broken_promise_to_operation_canceled)
        .then_unwrap(
            [client](auto&&)
            {
                if (client->failed())
                {
                    throw std::system_error(client->lastSysErrorCode(),
                        // TODO: Remove special case when we upgrade gcc.
                        // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60555
                        #if defined(__linux__) && defined(__GNUC__) && ( \
                            __GNUC__ < 8 \
                            || __GNUC__ == 8 && __GNUC_MINOR__ < 3 \
                            || __GNUC__ == 9 && __GNUC_MINOR__ < 1)
                            std::generic_category()
                        #else
                            std::system_category()
                        #endif
                    );
                }

                auto response = *client->response();
                response.messageBody = client->fetchMessageBodyBuffer();
                return response;
            });
}

} // namespace

cf::future<Response> Client::get(const Url& url)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doGet), this, url);
}

cf::future<Response> Client::head(const Url& url)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doHead), this, url);
}

cf::future<Response> Client::post(const Url& url)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doPost), this, url);
}

cf::future<Response> Client::post(const Url& url,
    std::unique_ptr<AbstractMsgBodySource>&& requestBody)
{
    setRequestBody(std::move(requestBody));
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doPost), this, url);
}

cf::future<Response> Client::post(const Url& url, StringType contentType, BufferType requestBody)
{
    return post(url, std::make_unique<BufferSource>(std::move(contentType), std::move(requestBody)));
}

cf::future<Response> Client::put(const Url& url)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doPut), this, url);
}

cf::future<Response> Client::put(const Url& url,
    std::unique_ptr<AbstractMsgBodySource>&& requestBody)
{
    setRequestBody(std::move(requestBody));
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doPut), this, url);
}

cf::future<Response> Client::put(const Url& url, StringType contentType, BufferType requestBody)
{
    return put(url, std::make_unique<BufferSource>(std::move(contentType), std::move(requestBody)));
}

cf::future<Response> Client::delete_(const Url& url)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doDelete), this, url);
}

cf::future<Response> Client::upgrade(const Url& url, const StringType& targetProtocol)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doUpgrade), this, url, targetProtocol);
}

cf::future<Response> Client::upgrade(const Url& url,
    const Method::ValueType& method, const StringType& targetProtocol)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doUpgrade), this, url, method, targetProtocol);
}

cf::future<Response> Client::connect(const Url& url, const StringType& targetHost)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doConnect), this, url, targetHost);
}

cf::future<Response> Client::request(Method::ValueType method, const Url& url)
{
    return futurize(NX_WRAP_MEM_FUNC_TO_LAMBDA(doRequest), this, method, url);
}

} // namespace nx::network::http::futures
