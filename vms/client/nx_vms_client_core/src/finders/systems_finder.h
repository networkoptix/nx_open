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
    QnSystemsFinder(QObject* parent = nullptr);

public: //overrides
    SystemDescriptionList systems() const override;
    QnSystemDescriptionPtr getSystem(const QString& systemId) const override;

    static QnSystemsFinder* instance();

protected:
    QnSystemsFinder(bool addDefaultFinders, QObject* parent);

    void initializeFinders();

    // TODO: #sivanov Change to enum class.
    enum Source
    {
        cloud,
        direct,
        recent,
        saved,
    };
    void addSystemsFinder(QnAbstractSystemsFinder* finder, Source source);

    bool mergeSystemIntoExisting(const QnSystemDescriptionPtr& system, Source source);

    void onBaseSystemDiscovered(const QnSystemDescriptionPtr& system, Source source);
    void onSystemLost(const QString& systemId, const nx::Uuid& localId, Source source);

protected:
    using SystemsFinderList = QMap<QnAbstractSystemsFinder*, nx::utils::ScopedConnectionsPtr>;
    using AggregatorPtr = QSharedPointer<QnSystemDescriptionAggregator>;
    using AggregatorsList = QHash<QString, AggregatorPtr>;

    SystemsFinderList m_finders;
    AggregatorsList m_systems;
};

#define qnSystemsFinder QnSystemsFinder::instance()
