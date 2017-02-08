
#pragma once

#include <finders/abstract_systems_finder.h>

class QnTestSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    QnTestSystemsFinder(QObject* parent);

    void startTest();

public: // overrides
    virtual SystemDescriptionList systems() const override;

    virtual QnSystemDescriptionPtr getSystem(const QString &id) const override;

    void addSystem(const QnSystemDescriptionPtr& system);

    void removeSystem(const QString& id);

    void clearSystems();

signals:
    void beforeSystemRemoved();

private:
    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    SystemsHash m_systems;
};

