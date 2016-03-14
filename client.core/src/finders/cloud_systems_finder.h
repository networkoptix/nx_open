
#pragma once

#include <nx/utils/thread/mutex.h>
#include <utils/common/connective.h>
#include <finders/abstract_systems_finder.h>
#include <watchers/cloud_status_watcher.h>

class QnCloudSystemsFinder : public Connective<QnAbstractSystemsFinder>
{
    Q_OBJECT
    typedef Connective<QnAbstractSystemsFinder> base_type;
    
public:
    QnCloudSystemsFinder(QObject *parent = nullptr);

    virtual ~QnCloudSystemsFinder();

public: // overrides
    SystemDescriptionList systems() const override;

private:
    void onCloudStatusChanged(QnCloudStatusWatcher::Status status);

    void setCloudSystems(const QnCloudSystemList &systems);

    void onCloudError(QnCloudStatusWatcher::ErrorCode error);

    void onSystemDiscovered(const QnSystemDescriptionPtr &system);

private:
    typedef QHash<QnUuid, QnSystemDescriptionPtr> SystemsHash;
    mutable QnMutex m_mutex;
    SystemsHash m_systems;
};