
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

    void updateSystems();

    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    void setFinalSystems(const SystemsHash& newFinalSystems);

    SystemsHash filterOutSystems(const SystemsHash& source);

    void removeFinalSystem(const QString& id);

private:
    typedef QPair<QnUuid, int> IdCountPair;
    typedef QHash<QString, IdCountPair> IdsDataHash;
    IdsDataHash m_filteringSystems;
    SystemsHash m_finalSystems;
};