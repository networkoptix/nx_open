
#pragma once

#include <functional>

#include <nx/utils/thread/mutex.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <utils/common/connective.h>
#include <network/system_description.h>
#include <finders/abstract_systems_finder.h>
#include <watchers/cloud_status_watcher.h>

class QTimer;

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

    void pingCloudSystem(const QString &cloudSystemId);

    void updateSystems();

    void tryRemoveAlienServer(const QnModuleInformation &serverInfo);

private:
    typedef QScopedPointer<QTimer> QTimerPtr;
    typedef QHash<QString, QnSystemDescription::PointerType> SystemsHash;
    typedef QHash<int, QString> RequestIdToSystemHash;

    const QTimerPtr m_updateSystemsTimer;
    mutable QnMutex m_mutex;

    SystemsHash m_systems;
};