#include "system_tiles_test_case.h"

#include <client/system_weights_manager.h>
#include <finders/test_systems_finder.h>
#include <network/local_system_description.h>

#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <helpers/system_weight_helper.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/raii_guard.h>

namespace {

static constexpr auto kRemoveSystemTimeoutMs = 10000;
static constexpr auto kNextActionPeriodMs = 3000;
static constexpr auto kImmediateActionDelayMs = 0;

void addServerToSystem(
    const QnSystemDescriptionPtr& baseSystem,
    const QUrl& url = QUrl::fromUserInput(lit("127.0.0.1:7001")),
    const QnSoftwareVersion& version = QnSoftwareVersion(QnAppInfo::applicationVersion()),
    int protoVersion = QnAppInfo::ec2ProtoVersion())
{
    const auto system = baseSystem.dynamicCast<QnLocalSystemDescription>();
    if (!system)
    {
        NX_ASSERT(false, "Wrong system descrition type");
        return;
    }

    const auto systemName = system->name();
    QnModuleInformation serverInfo;
    serverInfo.id = QUuid::createUuid();
    serverInfo.name = lit("Server of <%1>").arg(systemName);
    serverInfo.systemName = systemName;
    serverInfo.localSystemId = system->localId();
    serverInfo.protoVersion = protoVersion;
    serverInfo.version = version;
    serverInfo.port = 7001;

    system->addServer(serverInfo, 0);
    system->setServerHost(serverInfo.id, url);
}

void removeServerFromSystem(
    const QnSystemDescriptionPtr& baseSystem,
    const QnUuid& serverId)
{
    const auto system = baseSystem.dynamicCast<QnLocalSystemDescription>();
    if (!system)
    {
        NX_ASSERT(false, "Wrong system descrition type");
        return;
    }

    system->removeServer(serverId);

}

QnSystemDescriptionPtr createSystem(
    const QUrl& url = QUrl::fromUserInput(lit("127.0.0.1:7001")),
    const QnSoftwareVersion& version = QnSoftwareVersion(QnAppInfo::applicationVersion()),
    int protoVersion = QnAppInfo::ec2ProtoVersion())
{
    static int systemNumber = 0;

    const auto number = ++systemNumber;

    const auto systemName = lit("System #%1").arg(QString::number(number));
    const auto systemId = systemName;
    const auto localSystemId = QUuid::createUuid();
    const auto system = QnLocalSystemDescription::create(systemId, localSystemId, systemName);
    addServerToSystem(system, url, version, protoVersion);

    return system;
}

qreal getSystemMaxWeight()
{
    qreal result = 0;
    for(const auto& weightData: qnSystemWeightsManager->weights())
    {
        const auto weight = nx::client::core::helpers::calculateSystemWeight(
            weightData.weight, weightData.lastConnectedUtcMs);

        if (result < weight)
            result = weight;
    }
    return result;
}

QnRaiiGuardPtr addSystem(
    const QnSystemDescriptionPtr& system,
    QnTestSystemsFinder* finder)
{
    finder->addSystem(system);
    return QnRaiiGuard::createDestructible(
        [system, finder]
        {
            finder->removeSystem(system->id());
        });
}

qreal setSystemMaxWeight(const QnSystemDescriptionPtr& system)
{
    const auto weight = getSystemMaxWeight() + 1;
    qnSystemWeightsManager->setWeight(system->localId(), weight);
    return weight;
}

QnRaiiGuardPtr makeCompletionGaurd(
    QnTileTest id,
    const QnSystemTilesTestCase::CompletionHandler& handler)
{
    return QnRaiiGuard::createDestructible(
        [id, handler]()
        {
            if (handler)
                handler(id);
        });
}

QString getMessage(QnTileTest test)
{
    static const QHash<int, QString> kMessages =
        {
            { static_cast<int>(QnTileTest::ChangeWeightOnCollapse),
                lit("change wight on tile collapse")},
            { static_cast<int>(QnTileTest::ChangeVersion),
                lit("change version of system")},
            { static_cast<int>(QnTileTest::MaximizeAppOnCollapse),
                lit("maximize app on tile collapse")},
            { static_cast<int>(QnTileTest::SwitchPage),
                lit("Switch page on tile collpase")}
        };

    const auto it = kMessages.find(static_cast<int>(test));
    if (it == kMessages.end())
    {
        NX_ASSERT(false, "No message for specified tile test");
        return QString();
    }

    return it.value();
}

} // namespace

QnSystemTilesTestCase::QnSystemTilesTestCase(
    QnTestSystemsFinder* finder,
    QObject* parent)
    :
    base_type(parent),
    m_finder(finder)
{
}


void QnSystemTilesTestCase::startTest(
    QnTileTest test,
    int delay,
    CompletionHandler completionHandler)
{
    const auto runTest =
        [this, test, completionHandler]()
        {
            switch(test)
            {
                case QnTileTest::ChangeWeightOnCollapse:
                    changeWeightsTest(completionHandler);
                    break;
                case QnTileTest::MaximizeAppOnCollapse:
                    maximizeTest(completionHandler);
                    break;
                case QnTileTest::ChangeVersion:
                    versionChangeTest(completionHandler);
                    break;
            case QnTileTest::SwitchPage:
                    switchPageTest(completionHandler);
                    break;
                default:
                    NX_ASSERT(false, "Wrong tile test identifier");
                    break;
            }
        };

    executeDelayedParented(runTest, delay, this);
}

void QnSystemTilesTestCase::changeWeightsTest(CompletionHandler completionHandler)
{
    const auto completionGuard = makeCompletionGaurd(
        QnTileTest::ChangeWeightOnCollapse, completionHandler);

    const auto firstSystem = createSystem();
    const auto secondSystem = createSystem();
    const auto secondWeight = setSystemMaxWeight(secondSystem);
    const auto firstWeight = setSystemMaxWeight(firstSystem);

    const auto firstSystemRemoveGuard = addSystem(firstSystem, m_finder);
    const auto secondSystemRemoveGuard = addSystem(secondSystem, m_finder);

    const auto setSystemsWeights =
        [firstLocalId = firstSystem->localId(), secondLocalId = secondSystem->localId()]
            (qreal firstValue, qreal secondValue)
        {
            qnSystemWeightsManager->setWeight(firstLocalId, firstValue);
            qnSystemWeightsManager->setWeight(secondLocalId, secondValue);
        };

    setSystemsWeights(firstWeight, secondWeight);

    const auto delayedRemoveSystems = QnRaiiGuard::createDestructible(
        [this, firstSystemRemoveGuard, secondSystemRemoveGuard, completionGuard]()
        {
            executeDelayedParented(
                [firstSystemRemoveGuard, secondSystemRemoveGuard, completionGuard](){},
                kRemoveSystemTimeoutMs, this);
        });

    const auto swapWeights =
        [setSystemsWeights, firstWeight, secondWeight, delayedRemoveSystems]()
        {
            setSystemsWeights(secondWeight, firstWeight);
        };

    const auto collapseTile =
        [this, swapWeights, delayedRemoveSystems]()
        {
            collapseExpandedTile();
            executeDelayedParented(swapWeights, 100, this);
        };

    const auto secondSystemId = secondSystem->id();
    const auto expandTile =
        [this, collapseTile, delayedRemoveSystems, secondSystemId]()
        {
            emit openTile(secondSystemId);
            executeDelayedParented(collapseTile, kNextActionPeriodMs, this);
        };

    executeDelayedParented(expandTile, kNextActionPeriodMs, this);
}


void QnSystemTilesTestCase::maximizeTest(CompletionHandler completionHandler)
{
    const auto completionGuard = makeCompletionGaurd(
        QnTileTest::MaximizeAppOnCollapse, completionHandler);

    const auto system = createSystem();
    const auto systemRemoveGuard = addSystem(system, m_finder);
    setSystemMaxWeight(system);

    openTile(system->id());
    emit restoreApp();

    const auto collapseTile =
        [this, systemRemoveGuard, completionGuard]()
        {
            emit collapseExpandedTile();
            emit makeFullscreen();

            executeDelayedParented([systemRemoveGuard, completionGuard](){},
                kRemoveSystemTimeoutMs, this);
        };

    executeDelayedParented(collapseTile, kNextActionPeriodMs, this);
}

void QnSystemTilesTestCase::versionChangeTest(CompletionHandler completionHandler)
{
    const auto completionGuard = makeCompletionGaurd(
        QnTileTest::ChangeVersion, completionHandler);

    const auto system = createSystem(QUrl::fromUserInput(lit("127.0.0.1:7002")),
        QnSoftwareVersion(2, 6), QnAppInfo::ec2ProtoVersion() - 10);
    setSystemMaxWeight(system);
    const auto systemRemoveGuard = addSystem(system, m_finder);

    const auto changeServer =
        [this, system, systemRemoveGuard, completionGuard]()
        {
            removeServerFromSystem(system, system->servers().first().id);
            addServerToSystem(system);

            executeDelayedParented([systemRemoveGuard, completionGuard](){},
                kRemoveSystemTimeoutMs, this);
        };

    executeDelayedParented(changeServer, kNextActionPeriodMs, this);
}

void QnSystemTilesTestCase::switchPageTest(CompletionHandler completionHandler)
{
    const auto completionGuard = makeCompletionGaurd(
        QnTileTest::SwitchPage, completionHandler);

    QList<QnRaiiGuardPtr> systemRemoveGuards;

    // We have 8 tiles per page as maximum. So, to have 2 pages we need 9 tiles.
    static constexpr auto kTilesCountEnoughForTowPages = 9;
    QnSystemDescriptionPtr maxWeightSystem;
    for (auto i = 0; i != kTilesCountEnoughForTowPages; ++i)
    {
        maxWeightSystem = createSystem();
        setSystemMaxWeight(maxWeightSystem);
        systemRemoveGuards.append(addSystem(maxWeightSystem, m_finder));
    }

    const auto switchToFirstPage =
        [this, completionGuard, systemRemoveGuards]()
        {
            switchPage(0);
            executeDelayedParented([completionGuard, systemRemoveGuards](){},
                kRemoveSystemTimeoutMs, this);
        };

    const auto switchToSecondPage =
        [this, switchToFirstPage]()
        {
            emit switchPage(1);
            executeDelayedParented(switchToFirstPage, kNextActionPeriodMs, this);
        };
    const auto collapseTileCallback =
        [this, switchToSecondPage]()
        {
            emit collapseExpandedTile();
            executeDelayedParented(switchToSecondPage, kImmediateActionDelayMs, this);
        };

    const auto openTileCallback =
        [this, collapseTileCallback, id = maxWeightSystem->id()]()
        {
            emit openTile(id);
            executeDelayedParented(collapseTileCallback, kNextActionPeriodMs, this);
        };

    executeDelayedParented(openTileCallback, kNextActionPeriodMs, this);
}

void QnSystemTilesTestCase::showMessageDelayed(const QString& message)
{
    const auto showMessage =
        [this, message](){ emit messageChanged(message);};

    executeDelayedParented(showMessage, kNextActionPeriodMs, this);
}

void QnSystemTilesTestCase::runTestSequence(
    QnTileTest current,
    int delay)
{
    const auto completionHandler =
        [this, delay](QnTileTest test)
        {
            const auto nextTest = static_cast<QnTileTest>(static_cast<int>(test) + 1);
            if (nextTest == QnTileTest::Count)
            {
                showMessageDelayed(QString());
                return;
            }

            emit messageChanged(lit("Starting test: %1").arg(getMessage(nextTest)));
            showMessageDelayed(QString());
            runTestSequence(nextTest, delay);
        };

    startTest(current, delay, completionHandler);
}
