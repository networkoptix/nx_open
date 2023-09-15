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

private:
    using SystemsFinderList = QMap<QnAbstractSystemsFinder*, nx::utils::ScopedConnectionsPtr>;
    using AggregatorPtr = QSharedPointer<QnSystemDescriptionAggregator>;
    using AggregatorsList = QHash<QnUuid, AggregatorPtr>;

    SystemsFinderList m_finders;
    AggregatorsList m_systems;
    QHash<QString, QnUuid> m_systemToLocalId;
};

#define qnSystemsFinder QnSystemsFinder::instance()
