
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

    void checkAllSystems();

    void checkSystem(const QnSystemDescriptionPtr& system);

    void removeVisibleSystem(const QString& systemId);

    bool shouldRemoveSystem(const QnSystemDescriptionPtr& system);

private:
    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    typedef QHash<QString, QString> SystemNamesHash;

    SystemsHash m_systems;
    SystemsHash m_reservedSystems;
    SystemNamesHash m_onlineSystems;
};