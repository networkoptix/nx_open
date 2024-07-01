// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <finders/abstract_systems_finder.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/singleton.h>

class QnSystemDescriptionAggregator;
using SystemDescriptionAggregatorPtr = QSharedPointer<QnSystemDescriptionAggregator>;

/**
 * Central place for all discovered systems information. Collects data from different sources and
 * provides access to the united data, as well as notifications about changes.
 *
 * This class was designed for the purposes of connecting to systems, so it is used as a primary
 * source of data for the Welcome Screen and for the cloud system list in the Resources Tree.
 *
 * IMPORTANT: "id" of the system, which is used as a primary key here, is not actually an ID (by
 * the current implementation). Issue here is that physical systems really have no single unique ID
 * at all. So we are using Local System ID (of type Uuid) for the systems, which are not connected
 * to the cloud, and Cloud System ID (string) - for those which are connected. Also there is a
 * separate algorithm for the Factory systems.
 * That effectively means the following:
 * * System ID is not persistent. At the moment a system is bound to the cloud or unbound from it,
 *     QnSystemsFinder will emit `systemLost()` and then `systemDiscovered()` with a new ID.
 * * There can be several different systems, available for the client at the same time, which have
 *     the same Local System ID. The situation can be achieved with a multiple ways, and we have
 *     real customers with it. When all these systems are bound to the cloud, client still can work
 *     with them simultaneously (as Cloud System ID is used). When some of them are not - issues
 *     are expected.
 * * When factory system is setup, its ID is also changed (as new Local ID is assigned).
 *
 * Id for the certain system can be calculated with `helpers::getTargetSystemId()` function.
 */
class NX_VMS_CLIENT_CORE_API QnSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    using base_type = QnAbstractSystemsFinder;

public:
    QnSystemsFinder(QObject* parent = nullptr);

    SystemDescriptionList systems() const override;
    QnSystemDescriptionPtr getSystem(const QString& systemId) const override;

    // TODO: #sivanov Change to enum class and move to the QnSystemDescriptionAggregator logic.
    enum Source
    {
        cloud,
        direct,
        recent,
        saved,
    };

    static QnSystemsFinder* instance();

protected:
    QnSystemsFinder(bool addDefaultFinders, QObject* parent);


    void initializeFinders();

    void addSystemsFinder(QnAbstractSystemsFinder* finder, Source source);

    bool mergeSystemIntoExisting(const QnSystemDescriptionPtr& system, Source source);
    SystemDescriptionAggregatorPtr createNewAggregator(
        const QnSystemDescriptionPtr& system,
        Source source) const;
    bool systemIsHidden(const QnSystemDescriptionPtr& system, Source source) const;

    void onBaseSystemDiscovered(const QnSystemDescriptionPtr& system, Source source);
    void onSystemLost(const QString& systemId, const nx::Uuid& localId, Source source);

protected:
    QMap<QnAbstractSystemsFinder*, nx::utils::ScopedConnectionsPtr> m_finders;
    QHash<QString, SystemDescriptionAggregatorPtr> m_systems;
};

#define qnSystemsFinder QnSystemsFinder::instance()
