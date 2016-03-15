
#pragma once

#include <functional>
#include <QtCore/QSharedPointer>

class QnRaiiGuard;
typedef QSharedPointer<QnRaiiGuard> QnRaiiGuardPtr;

class NX_UTILS_API QnRaiiGuard
{
public:
    typedef std::function<void ()> Handler;

    static QnRaiiGuardPtr create(const Handler &creationHandler
        , const Handler &destructionHandler);

    static QnRaiiGuardPtr createDestructable(const Handler &destructionHandler);

    static QnRaiiGuardPtr createEmpty();

    explicit QnRaiiGuard(const Handler &creationHandler
        , const Handler &destructionHandler);

    ~QnRaiiGuard();

private:
    const Handler m_destructionHandler;
};