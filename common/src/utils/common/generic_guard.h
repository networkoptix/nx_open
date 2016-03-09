
#pragma once

#include <functional>

class QnGenericGuard
{
public:
    typedef std::function<void ()> Handler;
    typedef std::shared_ptr<QnGenericGuard> PointerType;

    static PointerType create(const Handler &creationHandler
        , const Handler &destructionHandler);

    static PointerType createEmpty();

    explicit QnGenericGuard(const Handler &creationHandler
        , const Handler &destructionHandler);

    ~QnGenericGuard();

private:
    const Handler m_destructionHandler;

};