// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <finders/abstract_systems_finder.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/singleton.h>

class ConnectionsHolder;
class QnSystemDescriptionAggregator;

class NX_VMS_CLIENT_CORE_API QnSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    explicit QnSystemsFinder(QObject* parent = nullptr);
    virtual ~QnSystemsFinder();

    void addSystemsFinder(QnAbstractSystemsFinder* finder, int priority);

public: //overrides
    SystemDescriptionList systems() const override;
    QnSystemDescriptionPtr getSystem(const QString& systemId) const override;

    static QnSystemsFinder* instance();

private:
    void onBaseSystemDiscovered(const QnSystemDescriptionPtr& system, int priority);
    void onSystemLost(const QString& systemId, int priority);

public:
    // LocalID is not a unique system ID. A user can have multiple systems locally (for example, for
    // QA purposes), and each of them can be linked to the cloud under a different Cloud ID. There
    // are different ways to do this, such as cloning a virtual machine with a system installed. We
    // will use a pair of locaID and CloudID to uniquely identify a system tile.
    struct SystemTileId
    {
        QnUuid localId;
        QString cloudId;

        bool operator==(const SystemTileId&) const = default;
    };

private:
    using SystemsFinderList = QMap<QnAbstractSystemsFinder*, nx::utils::ScopedConnectionsPtr>;
    using AggregatorPtr = QSharedPointer<QnSystemDescriptionAggregator>;
    using AggregatorsList = QHash<SystemTileId, AggregatorPtr>;

    SystemsFinderList m_finders;
    AggregatorsList m_systems;
    QHash<QString, QnUuid> m_systemToLocalId;
};

inline size_t qHash(const QnSystemsFinder::SystemTileId& key, size_t seed = 0)
{
    return qHashMulti(seed, key.localId, key.cloudId);
}

#define qnSystemsFinder QnSystemsFinder::instance()
