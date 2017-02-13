
#pragma once

#include <nx/utils/singleton.h>
#include <nx/utils/disconnect_helper.h>
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
    typedef QMap<QnAbstractSystemsFinder *, QnDisconnectHelperPtr> SystemsFinderList;
    typedef QSharedPointer<QnSystemDescriptionAggregator> AggregatorPtr;
    typedef QHash<QString, AggregatorPtr> AggregatorsList;

    SystemsFinderList m_finders;
    AggregatorsList m_systems;
};


#define qnSystemsFinder QnSystemsFinder::instance()
