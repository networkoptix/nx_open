
#include "generic_guard.h"

QnRaiiGuardPtr QnRaiiGuard::create(const Handler &creationHandler
    , const Handler &destructionHandler)
{
    return QnRaiiGuardPtr(new QnRaiiGuard(creationHandler, destructionHandler));
}

QnRaiiGuardPtr QnRaiiGuard::createEmpty()
{
    return QnRaiiGuardPtr();
}

QnRaiiGuard::QnRaiiGuard(const Handler &creationHandler
    , const Handler &destructionHandler)
    : m_destructionHandler(destructionHandler)
{
    if (creationHandler)
        creationHandler();
}

QnRaiiGuard::~QnRaiiGuard()
{
    if (m_destructionHandler)
        m_destructionHandler();
}
