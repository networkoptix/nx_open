// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <finders/abstract_systems_finder.h>

/**
 * This class is a common base for ScopeLocalSystemsFinder and QnRecentLocalSystemsFinder.
 * It contains logic of getting diff between two sets of systems and sending corresponding signals.
 */
class LocalSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    using base_type = QnAbstractSystemsFinder;

public:
    LocalSystemsFinder(QObject* parent = nullptr);
    virtual ~LocalSystemsFinder() = default;

    virtual SystemDescriptionList systems() const override;
    virtual QnSystemDescriptionPtr getSystem(const QString& id) const override;

protected:
    virtual void updateSystems() = 0;

    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    void setSystems(const SystemsHash& newSystems);

private:
    void removeSystem(const QString& id);

private:
    SystemsHash m_systems;
};
