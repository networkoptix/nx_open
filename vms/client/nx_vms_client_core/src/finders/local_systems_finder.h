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

public: // overrides
    virtual SystemDescriptionList systems() const override;
    virtual QnSystemDescriptionPtr getSystem(const QString& id) const override;

protected:
    virtual void updateSystems() = 0;

    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    void setFinalSystems(const SystemsHash& newFinalSystems);

    void removeFinalSystem(const QString& id);

private:
    // Recent systems that have no online ones
    SystemsHash m_finalSystems;
};
