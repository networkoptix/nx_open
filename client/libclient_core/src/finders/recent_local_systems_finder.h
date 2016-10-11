
#pragma once

#include <finders/abstract_systems_finder.h>
#include <network/system_description.h>

class QnRecentLocalSystemsFinder : public QnAbstractSystemsFinder
{
    Q_OBJECT
    using base_type = QnAbstractSystemsFinder;

public:
    QnRecentLocalSystemsFinder(QObject* parent = nullptr);

    virtual ~QnRecentLocalSystemsFinder() = default;

    void processSystemAdded(const QnSystemDescriptionPtr& system);

    void processSystemRemoved(const QString& systemId);

public: // overrides
    virtual SystemDescriptionList systems() const override;

    virtual QnSystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void updateSystems();

    bool isFilteredOut(const QnSystemDescriptionPtr& system) const;

    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    void updateRecentSystems(const SystemsHash& newSystems);

private:
    typedef QHash<QString, int> NameCountHash;
    typedef QHash<QString, QString> IdNameHush;

    SystemsHash m_recentSystems;
    IdNameHush m_onlineSystems;
    NameCountHash m_onlineSystemNames;
    SystemsHash m_finalSystems;
};