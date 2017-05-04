#include "startup_tile_manager.h"

#include <functional>

#include <QtCore/QTimer>
#include <QtCore/QPointer>

#include <finders/systems_finder.h>
#include <client_core/client_core_settings.h>
#include <nx/utils/log/assert.h>

QnStartupTileManager::QnStartupTileManager():
    base_type(),
    m_actionEmitted(false)
{
    NX_ASSERT(qnSystemsFinder, "Systems finder is nullptr");
    if (!qnSystemsFinder)
        return;

    NX_ASSERT(qnClientCoreSettings, "Client core settings is nullptr");
    if (!qnClientCoreSettings)
        return;

    /**
    * We've found several systems last time. Automatic tile management
    * will be turned off forever.
    */
    if (qnClientCoreSettings->skipStartupTilesManagement())
        return;

    QTimer::singleShot(qnClientCoreSettings->startupDiscoveryPeriodMs(),
        [this, guard = QPointer<QObject>(this)]()
        {
            if (guard)
                handleFirstSystems();
        });

    const auto checkForCancelAndUninitialize =
        [this]()
        {
            if (qnClientCoreSettings->skipStartupTilesManagement()
                || (qnSystemsFinder->systems().size() > 1))
            {
                cancelAndUninitialize();
            }
        };

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this, checkForCancelAndUninitialize](int value)
        {
            if (value == QnClientCoreSettings::SkipStartupTilesManagement)
                checkForCancelAndUninitialize();
        });

    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
        this, checkForCancelAndUninitialize);
}

void QnStartupTileManager::handleFirstSystems()
{
    if (qnClientCoreSettings->skipStartupTilesManagement())
        return;

    const auto currentSystems = qnSystemsFinder->systems();
    if (currentSystems.isEmpty())
    {
        // Will open first discovered system later
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered, this,
            [this](const QnSystemDescriptionPtr& system) { emitTileAction(system, false); });
    }
    else if (currentSystems.size() == 1)
    {
        // Throw system tile action immediately
        emitTileAction(currentSystems.first(), true);
    }
    else
    {
        // We've found more than 1 system - cancel next startup tile management
        cancelAndUninitialize();
    }
}

void QnStartupTileManager::skipTileAction()
{
    m_actionEmitted = true;
}

void QnStartupTileManager::emitTileAction(
    const QnSystemDescriptionPtr& system,
    bool initial)
{
    if (m_actionEmitted || qnClientCoreSettings->skipStartupTilesManagement())
        return;

    emit tileActionRequested(system->id(), initial);
}

void QnStartupTileManager::cancelAndUninitialize()
{
    disconnect(qnClientCoreSettings, nullptr, this, nullptr);
    disconnect(qnSystemsFinder, nullptr, this, nullptr);

    qnClientCoreSettings->setSkipStartupTilesManagement(true);
    qnClientCoreSettings->save();
}
