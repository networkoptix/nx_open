#include "cluster_manager.h"

namespace ec2
{

static QnClusterManager* globalInstance = 0;

QnClusterManager* QnClusterManager::instance()
{
    return globalInstance;
}

void QnClusterManager::initStaticInstance(QnClusterManager* value)
{
    globalInstance = value;
}


}
