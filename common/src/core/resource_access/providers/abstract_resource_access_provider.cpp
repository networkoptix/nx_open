#include "abstract_resource_access_provider.h"

QnAbstractResourceAccessProvider::QnAbstractResourceAccessProvider(QObject* parent):
    base_type(parent),
    QnUpdatable()
{
}

QnAbstractResourceAccessProvider::~QnAbstractResourceAccessProvider()
{
}
