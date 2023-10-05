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
