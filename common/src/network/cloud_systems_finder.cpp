
#include "cloud_systems_finder.h"

#include <common/common_module.h>

QnCloudSystemsFinder::QnCloudSystemsFinder(QObject *parent)
    : base_type(parent)
{
//    qnCommon->instance<QnCloudStatusWatcher>();
}

QnAbstractSystemsFinder::SystemDescriptionList QnCloudSystemsFinder::systems() const
{
    return SystemDescriptionList();
}

void QnCloudSystemsFinder::onConnectedToCloud(const QString &userName)
{}

void QnCloudSystemsFinder::onDisconnectedFromCloud()
{}
