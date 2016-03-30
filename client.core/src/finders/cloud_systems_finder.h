
#pragma once

#include <functional>

#include <nx/utils/thread/mutex.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <utils/common/connective.h>
#include <finders/abstract_systems_finder.h>
#include <watchers/cloud_status_watcher.h>

class QTimer;
struct QnConnectionInfo;

class QnCloudSystemsFinder : public Connective<QnAbstractSystemsFinder>
{
    Q_OBJECT
    typedef Connective<QnAbstractSystemsFinder> base_type;
    
public:
    QnCloudSystemsFinder(QObject *parent = nullptr);

    virtual ~QnCloudSystemsFinder();

public: // overrides
    SystemDescriptionList systems() const override;

    QnSystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void onCloudStatusChanged(QnCloudStatusWatcher::Status status);

    void setCloudSystems(const QnCloudSystemList &systems);

    void onCloudError(QnCloudStatusWatcher::ErrorCode error);
    
    void updateSystemInternal(const QnSystemDescriptionPtr &system);

    void pingServerInternal(const QString &host
        , int serverPriority
        , const QString &systemId);

    void checkOutdatedServersInternal(const QnSystemDescriptionPtr &system);

    void updateSystems();

    void tryRemoveAlienServer(const QnModuleInformation &serverInfo
        , const QString &supposedSystemId);

private:
    typedef QScopedPointer<QTimer> QTimerPtr;
    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    typedef QHash<int, QString> RequestIdToSystemHash;

    const QTimerPtr m_updateSystemsTimer;
    mutable QnMutex m_mutex;

    SystemsHash m_systems;
    RequestIdToSystemHash m_requestToSystem;
};