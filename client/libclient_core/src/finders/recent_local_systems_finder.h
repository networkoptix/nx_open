
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

    virtual SystemDescriptionList systems() const override;

    virtual QnSystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void updateSystems();

    typedef QList<QnModuleInformation> ModulesList;
    void checkSystem(const QnSystemDescriptionPtr& system,
        const ModulesList& modules);

    void removeVisibleSystem(const QString& systemId);

    bool shouldRemoveSystem(const QnSystemDescriptionPtr& system,
        const ModulesList& modules);

private:
    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    SystemsHash m_systems;
    SystemsHash m_onlineSystems;
};