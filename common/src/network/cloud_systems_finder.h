
#pragma once

#include <network/abstract_systems_finder.h>

class QnCloudSystemsFinder : public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;
public:
    QnCloudSystemsFinder(QObject *parent = nullptr);


public: // overrides
    SystemDescriptionList systems() const override;

private:
    void onConnectedToCloud(const QString &userName);

    void onDisconnectedFromCloud();

};