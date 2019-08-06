#include "abstract_resource_access_provider.h"
#include <core/resource_access/resource_access_subject.h>

QnAbstractResourceAccessProvider::QnAbstractResourceAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(parent),
    QnUpdatable(),
    m_mode(mode)
{
}

QnAbstractResourceAccessProvider::~QnAbstractResourceAccessProvider()
{
}

nx::core::access::Mode QnAbstractResourceAccessProvider::mode() const
{
    return m_mode;
}
