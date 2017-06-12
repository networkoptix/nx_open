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
static constexpr auto kNextActionPeriodMs = 2000;
static constexpr auto kImmediateActionDelayMs = 0;

static const auto kCheckEmptyTileMessage =
    lit("Please check if there is an empty item on the screen");

int castTileToInt(QnTileTest test)
{
    return static_cast<int>(test);
}

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

    const auto systemName = lit("TILE TEST SYSTEM #%1").arg(QString::number(number));
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

QString getMessage(QnTileTest test)
{
    static const QHash<int, QString> kMessages =
        {
            { castTileToInt(QnTileTest::ChangeWeightOnCollapse),
                lit("change order of systems on tile collapse")},
            { castTileToInt(QnTileTest::ChangeVersion),
                lit("change version of system for single tile")},
            { castTileToInt(QnTileTest::MaximizeAppOnCollapse),
                lit("maximize app on tile collapse")},
            { castTileToInt(QnTileTest::SwitchPage),
                lit("switch page on tile collpase")}
        };

    const auto it = kMessages.find(castTileToInt(test));
    if (it == kMessages.end())
    {
        NX_ASSERT(false, "No message for specified tile test");
        return QString();
    }

    return it.value();
}

template<typename RemoveGuardType>
QnRaiiGuardPtr makeDelayedCompletionGuard(
    const QString& prefinalMessage,
    QnTileTest id,
    const QnSystemTilesTestCase::CompletionHandler& completionHandler,
    const RemoveGuardType& systemRemoveGuard,
    QnSystemTilesTestCase* parent)
{
    return QnRaiiGuard::createDestructible(
        [id, prefinalMessage, systemRemoveGuard, completionHandler, parent]()
        {
            emit parent->messageChanged(prefinalMessage);
            const auto finalizeCallback =
                [id, systemRemoveGuard, completionHandler]()
                {
                    if (completionHandler)
                        completionHandler(id);
                };
            executeDelayedParented(finalizeCallback, kRemoveSystemTimeoutMs, parent);
        });
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
    const auto firstSystem = createSystem();
    const auto secondSystem = createSystem();
    const auto secondWeight = setSystemMaxWeight(secondSystem);
    const auto firstWeight = setSystemMaxWeight(firstSystem);

    const auto firstSystemRemoveGuard = addSystem(firstSystem, m_finder);
    const auto secondSystemRemoveGuard = addSystem(secondSystem, m_finder);

    const auto completionGuard = makeDelayedCompletionGuard(kCheckEmptyTileMessage,
        QnTileTest::ChangeWeightOnCollapse, completionHandler,
        QList<QnRaiiGuardPtr>() << firstSystemRemoveGuard << secondSystemRemoveGuard, this);

    const auto setSystemsWeights =
        [firstLocalId = firstSystem->localId(), secondLocalId = secondSystem->localId()]
            (qreal firstValue, qreal secondValue)
        {
            qnSystemWeightsManager->setWeight(firstLocalId, firstValue);
            qnSystemWeightsManager->setWeight(secondLocalId, secondValue);
        };

    setSystemsWeights(firstWeight, secondWeight);

    const auto swapWeights =
        [setSystemsWeights, firstWeight, secondWeight, completionGuard]()
        {
            setSystemsWeights(secondWeight, firstWeight);
        };

    const auto collapseTile =
        [this, swapWeights]()
        {
            collapseExpandedTile();
            executeDelayedParented(swapWeights, 100, this);
        };

    const auto secondSystemId = secondSystem->id();
    const auto expandTile =
        [this, collapseTile, secondSystemId]()
        {
            emit openTile(secondSystemId);
            executeDelayedParented(collapseTile, kNextActionPeriodMs, this);
        };

    executeDelayedParented(expandTile, kNextActionPeriodMs, this);
}


void QnSystemTilesTestCase::maximizeTest(CompletionHandler completionHandler)
{
    const auto system = createSystem();
    const auto systemRemoveGuard = addSystem(system, m_finder);

    const auto completionGuard = makeDelayedCompletionGuard(kCheckEmptyTileMessage,
        QnTileTest::MaximizeAppOnCollapse, completionHandler, systemRemoveGuard, this);

    setSystemMaxWeight(system);

    openTile(system->id());
    emit restoreApp();

    const auto collapseTile =
        [this, completionGuard]()
        {
            emit collapseExpandedTile();
            emit makeFullscreen();
        };

    executeDelayedParented(collapseTile, kNextActionPeriodMs, this);
}

void QnSystemTilesTestCase::versionChangeTest(CompletionHandler completionHandler)
{
    const auto system = createSystem(QUrl::fromUserInput(lit("127.0.0.1:7002")),
        QnSoftwareVersion(2, 6), QnAppInfo::ec2ProtoVersion() - 10);
    setSystemMaxWeight(system);
    const auto systemRemoveGuard = addSystem(system, m_finder);

    const auto completionGuard = makeDelayedCompletionGuard(kCheckEmptyTileMessage,
        QnTileTest::ChangeVersion, completionHandler, systemRemoveGuard, this);

    const auto changeServer =
        [this, system, completionGuard]()
        {
            removeServerFromSystem(system, system->servers().first().id);
            addServerToSystem(system);
        };

    executeDelayedParented(changeServer, kNextActionPeriodMs, this);
}

void QnSystemTilesTestCase::switchPageTest(CompletionHandler completionHandler)
{
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

    const auto completionGuard = makeDelayedCompletionGuard(kCheckEmptyTileMessage,
        QnTileTest::SwitchPage, completionHandler, systemRemoveGuards, this);

    const auto switchToFirstPage =
        [this, completionGuard]()
        {
            switchPage(0);
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


void QnSystemTilesTestCase::showAutohideMessage(
    const QString& message,
    qint64 hideDelay)
{
    emit messageChanged(message);

    const auto hideMessage =
        [this, message](){ emit messageChanged(QString());};

    executeDelayedParented(hideMessage, hideDelay, this);
}

void QnSystemTilesTestCase::runTestSequence(
    QnTileTest current,
    int delay)
{
    if (current >= QnTileTest::Count)
    {
        showAutohideMessage(lit("Tests completed"), kNextActionPeriodMs * 2);
        return;
    }

    if (m_currentTileTest != QnTileTest::Count)
        return;

    const auto completionHandler =
        [this, delay](QnTileTest test)
        {
            const auto nextTest = static_cast<QnTileTest>(castTileToInt(test) + 1);
            m_currentTileTest = QnTileTest::Count;
            runTestSequence(nextTest, delay);
        };

    emit messageChanged(lit("Current test (%1 / %2): %3").arg(
        QString::number(castTileToInt(current) + 1),
        QString::number(castTileToInt(QnTileTest::Count)),
        getMessage(current)));

    m_currentTileTest = current;
    startTest(current, delay, completionHandler);
}
