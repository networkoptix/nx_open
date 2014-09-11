
#include "aux_manager.h"

#include "version.h"

#include "common/common_module.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{

static QnAuxManager* globalInstance = 0;


QnAuxManager::QnAuxManager()
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;
}

QnAuxManager::~QnAuxManager()
{
    globalInstance = 0;
}

QnAuxManager* QnAuxManager::instance()
{
    return globalInstance;
}


} // namespace ec2
