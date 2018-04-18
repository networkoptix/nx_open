#include "ec_api.h"

namespace ec2
{

AbstractECConnectionFactory::AbstractECConnectionFactory(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule) 
{
}

} // namespace ec2
