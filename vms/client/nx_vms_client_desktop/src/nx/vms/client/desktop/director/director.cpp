#include "director.h"

#include <QMetaEnum>
#include <QJSValueIterator>

#include <utils/common/delayed.h>

#include <common/common_module.h>

#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <client_core/client_core_module.h>

#include <nx/vms/client/desktop/system_logon/data/logon_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <nx/vms/client/desktop/debug_utils/instruments/debug_info_instrument.h>

#include <nx/fusion/serialization/lexical.h>

namespace nx::vmx::client::desktop {

using namespace vms::client::desktop::ui;
using LogonParameters = nx::vms::client::desktop::LogonParameters;

namespace {

using namespace std::chrono_literals;
std::chrono::milliseconds kQuitDelay = 2s;

const QString kItemsProperty = "items";
const QString kResourcesProperty = "resources";

LogonParameters stringToLogonParameters(QString s)
{
    return LogonParameters(s);
}

} // namespace

Director::Director(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    QMetaType::registerConverter<QString, LogonParameters>(stringToLogonParameters);
    NX_ASSERT_HEAVY_CONDITION(
        []
        {
            for (int i = Qn::FirstItemDataRole; i < Qn::ItemDataRoleCount; ++i)
            {
                if (QnLexical::serialized(static_cast<Qn::ItemDataRole>(i)).isEmpty())
                    return false;
            }
            return true;
        }(), "Missing lexical serialization for Qn::ItemDataRole");
}

Director::~Director()
{
}

void Director::quit(bool force)
{
    NX_ASSERT(context());

    if (force)
    {
        executeDelayedParented(
            [this] { context()->action(action::DelayedForcedExitAction)->trigger(); },
            kQuitDelay, this);
    }
    else
    {
        executeDelayedParented(
            [this] { context()->action(action::ExitAction)->trigger(); },
            kQuitDelay, this);
    }
}

std::vector<qint64> Director::getFrameTimePoints()
{
    NX_ASSERT(context());
    return context()->display()->debugInfoInstrument()->getFrameTimePoints();
}

void Director::setupJSEngine(QJSEngine* engine)
{
    m_engine = engine;
    m_engine->installExtensions(QJSEngine::ConsoleExtension);

    QJSValue actionEnumValue = m_engine->newQMetaObject(
        &nx::vms::client::desktop::ui::action::staticMetaObject);
    m_engine->globalObject().setProperty("action", actionEnumValue);
    m_engine->globalObject().setProperty("director", m_engine->newQObject(this));
}

void Director::subscribe(nx::vms::client::desktop::ui::action::IDType action, QJSValue callback)
{
    connect(context()->action(action), &QAction::triggered, this,
        [this, callback]() mutable
        {
            if (!callback.isCallable())
                return;

            const auto actionParameters = context()->menu()->currentParameters(sender());

            QJSValue param = m_engine->newObject();

            // Convert each action parameter to a named property of js object.
            QHashIterator<int, QVariant> i(actionParameters.arguments());
            while (i.hasNext())
            {
                i.next();
                // -1 is a special case for action items
                const QString name = (i.key() == -1)
                    ? kItemsProperty
                    : QnLexical::serialized(static_cast<Qn::ItemDataRole>(i.key()));
                param.setProperty(name, m_engine->toScriptValue<QVariant>(i.value()));
            }

            const QnResourceList resources = actionParameters.resources();
            const auto resorcesValue = resources.empty()
                    ? QVariant()
                    : QVariant::fromValue<QnResourceList>(resources);
            param.setProperty(kResourcesProperty, m_engine->toScriptValue<QVariant>(resorcesValue));

            callback.call(QJSValueList{param});
        });
}

void Director::trigger(nx::vms::client::desktop::ui::action::IDType action, QJSValue parameters)
{
    if (!parameters.isObject())
        return;

    action::Parameters actionParameters;

    // Gather js object own properties and map them to roles:
    //   "TextRole": value   ->   setArgument(Qn::TextRole, QVariant(value))
    QJSValueIterator it(parameters);
    while (it.hasNext())
    {
        it.next();
        if (it.name() == kResourcesProperty) // special case for action resources
        {
            actionParameters.setResources(it.value().toVariant().value<QnResourceList>());
            continue;
        }
        bool ok = false;
        const auto role = QnLexical::deserialized<Qn::ItemDataRole>(it.name(), Qn::ItemDataRoleCount, &ok);
        if (!ok || role == Qn::ItemDataRoleCount)
            continue;
        actionParameters.setArgument(role, it.value().toVariant());
    }

    context()->menu()->trigger(action, actionParameters);
}

QVariant Director::createResourcesForFiles(QStringList files)
{
    const auto acceptedFiles = QnFileProcessor::findAcceptedFiles(files);
    const auto resources = QnFileProcessor::createResourcesForFiles(acceptedFiles);
    if (resources.empty())
        return QVariant();
    return QVariant::fromValue<QnResourceList>(resources);
}

QVariant Director::getResourceById(QString id)
{
    const auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    auto resource = resourcePool->getResourceById(QnUuid::fromStringSafe(id));
    if (!resource)
        return QVariant();
    return QVariant::fromValue<QnResourcePtr>(resource);
}

} // namespace nx::vmx::client::desktop
