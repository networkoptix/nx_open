#include "db_manager.h"

namespace ec2
{

static QnDbManager* globalInstance = 0;

QnDbManager* QnDbManager::instance()
{
    return globalInstance;
}

void QnDbManager::initStaticInstance(QnDbManager* value)
{
    globalInstance = value;
}

}
