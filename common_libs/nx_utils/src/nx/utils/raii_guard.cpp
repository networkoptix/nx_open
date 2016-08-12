#include "raii_guard.h"

QnRaiiGuard::QnRaiiGuard(const Handler& creationHandler, const Handler& destructionHandler) :
    m_destructionHandler(destructionHandler)
{
    if (creationHandler)
        creationHandler();
}

QnRaiiGuard::QnRaiiGuard(QnRaiiGuard&& other) :
    m_destructionHandler(other.m_destructionHandler)
{
    other.m_destructionHandler = Handler();
}

QnRaiiGuard::~QnRaiiGuard()
{
    if (m_destructionHandler)
        m_destructionHandler();
}

QnRaiiGuardPtr QnRaiiGuard::create(const Handler& creationHandler, const Handler& destructionHandler)
{
    return QnRaiiGuardPtr(new QnRaiiGuard(creationHandler, destructionHandler));
}

QnRaiiGuardPtr QnRaiiGuard::createDestructable(const Handler& destructionHandler)
{
    return QnRaiiGuardPtr(new QnRaiiGuard(Handler(), destructionHandler));
}

QnRaiiGuardPtr QnRaiiGuard::createEmpty()
{
    return QnRaiiGuardPtr();
}
