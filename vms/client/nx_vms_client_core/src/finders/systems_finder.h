#pragma once

#include <QtCore/QSharedPointer>

#include <nx/utils/singleton.h>
#include <nx/utils/scoped_connections.h>
#include <finders/abstract_systems_finder.h>

class ConnectionsHolder;
class QnSystemDescriptionAggregator;

class QnSystemsFinder : public QnAbstractSystemsFinder
    , public Singleton<QnSystemsFinder>
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    explicit QnSystemsFinder(QObject *parent = nullptr);

    virtual ~QnSystemsFinder();

    void addSystemsFinder(QnAbstractSystemsFinder *finder,
        int priority);

public: //overrides
    SystemDescriptionList systems() const override;

    QnSystemDescriptionPtr getSystem(const QString& id) const override;

private:

    void onBaseSystemDiscovered(const QnSystemDescriptionPtr& system,
        int priority);

    void onSystemLost(const QString& systemId,
        int priority);

    void updateRecentConnections(const QnUuid& localSystemId, const QString& name);

private:
    using SystemsFinderList = QMap<QnAbstractSystemsFinder*, nx::utils::ScopedConnectionsPtr>;
    using AggregatorPtr = QSharedPointer<QnSystemDescriptionAggregator>;
    using AggregatorsList = QHash<QString, AggregatorPtr>;

    SystemsFinderList m_finders;
    AggregatorsList m_systems;
};


#define qnSystemsFinder QnSystemsFinder::instance()
