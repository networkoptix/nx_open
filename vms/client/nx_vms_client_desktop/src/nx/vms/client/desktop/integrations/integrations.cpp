#include "integrations.h"

#include <nx/vms/client/desktop/ini.h>

#include <nx/utils/singleton.h>

#include "interfaces.h"
#include "entropix/reconstruct_resolution.h"


namespace nx::vms::client::desktop::integrations {

namespace {

class Storage final: public QObject, public Singleton<Storage>
{
public:
    explicit Storage(QObject* parent):
        QObject(parent)
	{}

    void addIntegration(Integration* integration)
    {
        NX_ASSERT(integration->parent() == this);
        m_allIntegrations.push_back(integration);
        if (const auto actionFactory = dynamic_cast<IActionFactory*>(integration))
            m_actionFactories.push_back(actionFactory);
    }

    QList<IActionFactory*> actionFactories() const
    {
        return m_actionFactories;
    }

private:
    // Keeping raw pointers as all integrations are owned by this class.
    QList<Integration*> m_allIntegrations;
    QList<IActionFactory*> m_actionFactories;
};

} // namespace

void initialize(QObject* storageParent)
{
    NX_ASSERT(!Storage::instance());
    auto storage = new Storage(storageParent);

    QString enableEntropixZoomWindowReconstructionOn(ini().enableEntropixZoomWindowReconstructionOn);
    if (!enableEntropixZoomWindowReconstructionOn.isEmpty())
	{
        const auto modelsFilter = QRegularExpression(enableEntropixZoomWindowReconstructionOn);
		if (modelsFilter.isValid())
		{
            storage->addIntegration(new entropix::ReconstructResolutionIntegration(modelsFilter,
				storage));
		}
    }
}

void registerActions(ui::action::MenuFactory* factory)
{
    auto storage = Storage::instance();
    if (!NX_ASSERT(storage))
        return;

    for (const auto actionFactory: storage->actionFactories())
        actionFactory->registerActions(factory);
}

} // namespace nx::vms::client::desktop::integrations
