// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <finders/abstract_systems_finder.h>

class LocalSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    using base_type = QnAbstractSystemsFinder;

public:
    LocalSystemsFinder(QObject* parent = nullptr);
    virtual ~LocalSystemsFinder() = default;

    void processSystemAdded(const QnSystemDescriptionPtr& system);
    void processSystemRemoved(const QString& systemId, const QnUuid& localSystemId);

    virtual SystemDescriptionList systems() const override;
    virtual QnSystemDescriptionPtr getSystem(const QString &id) const override;

protected:
    virtual void updateSystems() = 0;

    typedef QHash<QnUuid, QnSystemDescriptionPtr> SystemsHash;

    void setSystems(const SystemsHash& newSystems);

private:
    SystemsHash filterOutSystems(const SystemsHash& source);
    void removeFilteredSystem(const QnUuid& id);

private:
    typedef QHash<QString, int> IdCountHash;
    typedef QHash<QnUuid, IdCountHash> IdsDataHash;

    // Hide from the finder systems, which discovered by any other finder (and thus are online).
    IdsDataHash m_nonLocalSystems;

    // Local systems that have no online ones.
    SystemsHash m_filteredSystems;

    QHash<QString, QnUuid> m_systemToLocalId;
};
