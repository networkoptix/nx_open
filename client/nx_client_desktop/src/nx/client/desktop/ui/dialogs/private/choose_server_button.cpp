#include "choose_server_button.h"

#include <QtWidgets/QMenu>

#include <ui/style/skin.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/assert.h>

namespace {

bool sameServer(const QnMediaServerResourcePtr& first, const QnMediaServerResourcePtr& second)
{
    return first && second && first->getId() == second->getId();
}

using SameServerPredicate = std::function<bool (const QnMediaServerResourcePtr&)>;
SameServerPredicate makeSameServerPredicate(const QnMediaServerResourcePtr& server)
{
    return
        [server](const QnMediaServerResourcePtr& other)
        {
            return sameServer(server, other);
        };
}

bool contains(const QnMediaServerResourceList& servers, const QnMediaServerResourcePtr& server)
{
    return std::any_of(servers.begin(), servers.end(), makeSameServerPredicate(server));
}

QString getText(const QnMediaServerResourcePtr& server)
{
    return lit("%1 (%2)").arg(server->getName(),
        server->getPrimaryAddress().address.toString());
}

QIcon getIcon(const QnMediaServerResourcePtr& server)
{
    return server->getStatus() == Qn::Online
        ? qnSkin->icon("tree/server.png")
        : qnSkin->icon("tree/server_offline.png");
}

} // namespace

QnChooseServerButton::QnChooseServerButton(QWidget* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    connect(this, &QnChooseServerButton::serverChanged,
        this, &QnChooseServerButton::serverOnlineStatusChanged);

    const auto pool = commonModule()->resourcePool();
    connect(pool, &QnResourcePool::resourceAdded, this, &QnChooseServerButton::tryAddServer);
    connect(pool, &QnResourcePool::resourceRemoved, this, &QnChooseServerButton::tryRemoveServer);

    for (const auto server: pool->getAllServers(Qn::AnyStatus))
        tryAddServer(server);

    connect(this, &QnChooseServerButton::serverChanged,
        this, &QnChooseServerButton::tryUpdateCurrentAction);
}

void QnChooseServerButton::tryUpdateCurrentAction()
{
    if (!m_server)
    {
        setCurrentAction(nullptr, false);
        return;
    }

    auto action = actionForServer(m_server);
    if (!action)
        action = addMenuItemForServer(m_server);

    if (action != currentAction())
    {
        setCurrentAction(action, false);
        emit serverOnlineStatusChanged();
    }
}

bool QnChooseServerButton::setServer(const QnMediaServerResourcePtr& server)
{
    if (sameServer(server, m_server))
        return false;

    if (server && !contains(m_servers, server))
    {
        NX_EXPECT(false, "Server does not exist");
        return false;
    }

    const auto previousServer = m_server;
    m_server = server;

    emit serverChanged(previousServer);

    updatebuttonData();
    return true;
}

QnMediaServerResourcePtr QnChooseServerButton::server() const
{
    return m_server;
}

int QnChooseServerButton::serversCount() const
{
    return m_servers.size();
}

bool QnChooseServerButton::serverIsOnline()
{
    const auto action = currentAction();
    if (!action)
        return false;

    const auto server = action->data().value<QnMediaServerResourcePtr>();
    return server && server->getStatus() == Qn::Online;
}

void QnChooseServerButton::updatebuttonData()
{
    if (!m_server)
    {
        if (!m_servers.isEmpty())
            setServer(m_servers.first());

        return;
    }

    setText(getText(m_server));
    setIcon(getIcon(m_server));
}

void QnChooseServerButton::tryAddServer(const QnResourcePtr& resource)
{
    const auto serverResource = resource.dynamicCast<QnMediaServerResource>();
    if (!serverResource)
        return;

    const auto serverSystemId = serverResource->getModuleInformation().localSystemId;
    const auto currentServer = commonModule()->currentServer();
    const auto currentSystemId = currentServer
        ? currentServer->getModuleInformation().localSystemId
        : QnUuid(); //< We don't have connection.
    if (currentSystemId != serverSystemId)
        return;

    if (contains(m_servers, serverResource))
    {
        NX_ASSERT(false, "Server already exists");
        return;
    }

    m_servers.append(serverResource);
    addMenuItemForServer(serverResource);

    emit serversCountChanged();
}

void QnChooseServerButton::tryRemoveServer(const QnResourcePtr& resource)
{
    const auto serverResource = resource.dynamicCast<QnMediaServerResource>();
    if (!serverResource)
        return;

    const auto it = std::find_if(m_servers.begin(), m_servers.end(),
        makeSameServerPredicate(serverResource));

    if (it == m_servers.end())
        return;

    if (const auto action = actionForServer(serverResource))
        removeAction(action);

    m_servers.erase(it);

    if (sameServer(m_server, serverResource))
    {
        setServer(m_servers.isEmpty()
            ? QnMediaServerResourcePtr()
            : m_servers.first());
    }

    emit serversCountChanged();
}

QAction* QnChooseServerButton::addMenuItemForServer(const QnMediaServerResourcePtr& server)
{
    const auto currentMenu = menu();
    const auto action = new QAction(currentMenu);
    action->setData(QVariant::fromValue(server));
    action->setText(getText(server));
    action->setIcon(getIcon(server));
    action->setIconVisibleInMenu(true);

    connect(server, &QnMediaServerResource::statusChanged, this,
        [this, action, server]()
        {
            const auto icon = getIcon(server);
            action->setIcon(icon);
            if (currentAction() != action)
                return;

            setIcon(icon);
            emit serverOnlineStatusChanged();
        });

    connect(server, &QnMediaServerResource::nameChanged, this,
        [action, server]() { action->setText(getText(server)); });
    connect(action, &QAction::triggered, this,
        [this, server]() { setServer(server); });

    currentMenu->addAction(action);
    return action;
}

QAction* QnChooseServerButton::actionForServer(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return nullptr;

    const auto currentMenu = menu();
    const auto actions = currentMenu->actions();
    const auto it = std::find_if(actions.begin(), actions.end(),
        [server](QAction* action)
        {
            return sameServer(server, action->data().value<QnMediaServerResourcePtr>());
        });

    return it == actions.end() ? nullptr : *it;
}
