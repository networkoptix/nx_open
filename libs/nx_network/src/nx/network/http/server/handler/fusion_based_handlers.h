#pragma once

#include <nx/utils/move_only_func.h>

#include "../abstract_fusion_request_handler.h"

namespace nx::network::http::server::handler {

template<typename ResultType>
class GetHandler:
    public nx::network::http::AbstractFusionRequestHandler<void, ResultType>
{
public:
    using FunctorType = nx::utils::MoveOnlyFunc<ResultType()>;

    GetHandler(FunctorType func):
        m_func(std::move(func))
    {
    }

private:
    FunctorType m_func;

    virtual void processRequest(
        nx::network::http::RequestContext /*requestContext*/) override
    {
        auto data = m_func();
        this->requestCompleted(
            nx::network::http::FusionRequestResult(),
            std::move(data));
    }
};

} // namespace nx::network::http::server::handler
