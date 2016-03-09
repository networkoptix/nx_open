
#include "generic_guard.h"

QnGenericGuard::PointerType QnGenericGuard::create(const Handler &creationHandler
    , const Handler &destructionHandler)
{
    return PointerType(new QnGenericGuard(creationHandler, destructionHandler));
}

QnGenericGuard::PointerType QnGenericGuard::createEmpty()
{
    return PointerType();
}

QnGenericGuard::QnGenericGuard(const Handler &creationHandler
    , const Handler &destructionHandler)
    : m_destructionHandler(destructionHandler)
{
    if (creationHandler)
        creationHandler();
}

QnGenericGuard::~QnGenericGuard()
{
    if (m_destructionHandler)
        m_destructionHandler();
}
