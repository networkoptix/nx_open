
#pragma once

#include <network/abstract_systems_finder.h>

class QnCloudSystemsFinder : public QnAbstractSystemsFinder
{
public:
    QnCloudSystemsFinder();

    void onConnectedToCloud(const QString &userName);

    void onDisconnectedFromCloud();

};