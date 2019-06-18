#include "integrations.h"

#include <nx/vms/client/desktop/ini.h>

#include <nx/utils/singleton.h>

#include "config.h"
#include "interfaces.h"

#include "entropix/reconstruct_resolution.h"
#include "internal/roi_visualization.h"

namespace nx::vms::client::desktop::integrations {

namespace {

class Storage final: public QObject, public Singleton<Storage>
{
public:
    // Keeping raw pointers as all integrations are owned by this class.
    QList<Integration*> allIntegrations;
    QList<IServerApiProcessor*> serverApiProcessors;
    QList<IMenuActionFactory*> actionFactories;
    QList<IOverlayPainter*> overlayPainters;

    explicit Storage(QObject* parent):
        QObject(parent)
	{}

    void addIntegration(Integration* integration)
    {
        NX_ASSERT(integration->parent() == this);
        allIntegrations.push_back(integration);
        if (const auto serverApiProcessor = dynamic_cast<IServerApiProcessor*>(integration))
            serverApiProcessors.push_back(serverApiProcessor);
        if (const auto actionFactory = dynamic_cast<IMenuActionFactory*>(integration))
            actionFactories.push_back(actionFactory);
        if (const auto overlayPainter = dynamic_cast<IOverlayPainter*>(integration))
            overlayPainters.push_back(overlayPainter);
    }
};

} // namespace

void initialize(QObject* storageParent)
{
    NX_ASSERT(!Storage::instance());
    auto storage = new Storage(storageParent);

    const QString entropixModelsFilter(config().enableEntropixZoomWindowReconstructionOn);
    if (!entropixModelsFilter.isEmpty())
	{
        const auto modelsFilter = QRegularExpression(entropixModelsFilter);
		if (modelsFilter.isValid())
		{
            storage->addIntegration(new entropix::ReconstructResolutionIntegration(modelsFilter,
				storage));
		}
    }

    if (config().enableRoiVisualization)
        storage->addIntegration(new internal::RoiVisualizationIntegration(storage));
}

void connectionEstablished(ec2::AbstractECConnectionPtr connection)
{
    auto storage = Storage::instance();
    if (!NX_ASSERT(storage))
        return;

    for (const auto serverApiProcessor: storage->serverApiProcessors)
        serverApiProcessor->connectionEstablished(connection);
}

void registerActions(ui::action::MenuFactory* factory)
{
    auto storage = Storage::instance();
    if (!NX_ASSERT(storage))
        return;

    for (const auto actionFactory: storage->actionFactories)
        actionFactory->registerActions(factory);
}

void registerWidget(QnMediaResourceWidget* widget)
{
    auto storage = Storage::instance();
    if (!NX_ASSERT(storage))
        return;

    for (const auto overlayPainter: storage->overlayPainters)
        overlayPainter->registerWidget(widget);
}

void unregisterWidget(QnMediaResourceWidget* widget)
{
    auto storage = Storage::instance();
    if (!NX_ASSERT(storage))
        return;

    for (const auto overlayPainter: storage->overlayPainters)
        overlayPainter->unregisterWidget(widget);
}

void paintVideoOverlays(QnMediaResourceWidget* widget, QPainter* painter)
{
    auto storage = Storage::instance();
    if (!NX_ASSERT(storage))
        return;

    for (const auto overlayPainter: storage->overlayPainters)
        overlayPainter->paintVideoOverlay(widget, painter);
}

} // namespace nx::vms::client::desktop::integrations
