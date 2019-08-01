#pragma once

#include <memory>

#include "ec_api_impl.h"

namespace ec2::impl {

template<typename UserHandler, typename... RequestCompletionHandlerArgs>
class RegularCompletionHandler:
    public AbstractHandler<RequestCompletionHandlerArgs...>
{
public:
    template<typename UserHandlerType>
    RegularCompletionHandler(UserHandlerType&& userHandler):
        m_userHandler(std::forward<UserHandlerType>(userHandler))
    {
    }

    virtual void done(int /*reqID*/, const RequestCompletionHandlerArgs&... args) override
    {
        m_userHandler(args...);
    }

private:
    UserHandler m_userHandler;
};

template<typename RequestCompletionHandler, typename UserHandler>
auto makeRegularCompletionHandler(UserHandler&& userHandler)
{
    return makeRegularCompletionHandlerImpl(
        std::forward<UserHandler>(userHandler),
        (RequestCompletionHandler*) nullptr);
}

template<typename UserHandler, typename... Args>
auto makeRegularCompletionHandlerImpl(
    UserHandler&& userHandler,
    AbstractHandler<Args...>*)
{
    return std::static_pointer_cast<AbstractHandler<Args...>>(
        std::make_shared<RegularCompletionHandler<UserHandler, Args...>>(
        std::forward<UserHandler>(userHandler)));
}

} // namespace ec2::impl
