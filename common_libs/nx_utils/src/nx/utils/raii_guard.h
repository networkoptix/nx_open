
#pragma once

#include <functional>

class QnRaiiGuard;
typedef std::shared_ptr<QnRaiiGuard> QnRaiiGuardPtr;

class QnRaiiGuard
{
public:
    typedef std::function<void ()> Handler;

    static QnRaiiGuardPtr create(const Handler &creationHandler
        , const Handler &destructionHandler);

    static QnRaiiGuardPtr createEmpty();

    explicit QnRaiiGuard(const Handler &creationHandler
        , const Handler &destructionHandler);

    ~QnRaiiGuard();

private:
    const Handler m_destructionHandler;

};