
#pragma once

#include <finders/abstract_systems_finder.h>
#include <network/local_system_description.h>

class QnRecentLocalSystemsFinder : public QnAbstractSystemsFinder
{
    Q_OBJECT
        using base_type = QnAbstractSystemsFinder;

public:
    QnRecentLocalSystemsFinder(QObject* parent = nullptr);

    virtual ~QnRecentLocalSystemsFinder() = default;

    void processSystemAdded(const QnSystemDescriptionPtr& system);

    void processSystemRemoved(const QString& systemId, const QnUuid& localSystemId);

public: // overrides
    virtual SystemDescriptionList systems() const override;

    virtual QnSystemDescriptionPtr getSystem(const QString &id) const override;

    void updateSystems();

    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    void setFinalSystems(const SystemsHash& newFinalSystems);

    SystemsHash filterOutSystems(const SystemsHash& source);

    void removeFinalSystem(const QString& id);

private:
    typedef QHash<QnUuid, int> IdCountHash;
    typedef QHash<QString, IdCountHash> IdsDataHash;

    // We don't allow to discover recent systems if we have online ones
    IdsDataHash m_filteringSystems;

    // Recent systems that have no online ones
    SystemsHash m_finalSystems;
};