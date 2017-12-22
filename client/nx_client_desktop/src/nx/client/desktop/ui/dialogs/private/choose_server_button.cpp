#include "choose_server_button.h"

#include <QtWidgets/QMenu>

#include <ui/style/skin.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/assert.h>

namespace {

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
    QnCommonModuleAware(parent),
    m_serverStatus(this)
{
}

bool QnChooseServerButton::setCurrentServer(const QnMediaServerResourcePtr& server)
{
    if (server && !actionForServer(server->getId()))
    {
        NX_EXPECT(false, "Can't find action for specified server");
        return false;
    }

    if (m_server == server)
        return false;

    const auto previous = m_server;
    m_server = server;
    setCurrentAction(actionForServer(m_server ? m_server->getId() : QnUuid()));

    emit currentServerChanged(m_server);
    return true;
}

QnMediaServerResourcePtr QnChooseServerButton::currentServer() const
{
    return m_server;
}

void QnChooseServerButton::addServer(const QnMediaServerResourcePtr& server)
{
    if (actionForServer(server->getId()))
    {
        NX_EXPECT(false, "Server exists already");
        return;
    }

    addMenuItemForServer(server);
}

void QnChooseServerButton::removeServer(const QnUuid& id)
{
    if (const auto action = actionForServer(id))
        menu()->removeAction(action);
    else
        NX_EXPECT(false, "Server does not exist");
}

void QnChooseServerButton::handleSelectedServerOnlineStatusChanged()
{
    const auto icon = getIcon(m_server);
    setIcon(icon);
}

QAction* QnChooseServerButton::addMenuItemForServer(const QnMediaServerResourcePtr& server)
{
    const auto currentMenu = menu();
    const auto action = new QAction(currentMenu);
    action->setData(QVariant::fromValue(server));
    action->setText(getText(server));
    action->setIcon(getIcon(server));
    action->setIconVisibleInMenu(true);

    connect(server, &QnMediaServerResource::nameChanged, this,
        [action, server]() { action->setText(getText(server)); });
    connect(server, &QnMediaServerResource::statusChanged, this,
        [action, server]() { action->setIcon(getIcon(server)); });
    connect(action, &QAction::triggered, this,
        [this, server]() { setCurrentServer(server); });

    currentMenu->addAction(action);
    return action;
}

QAction* QnChooseServerButton::actionForServer(const QnUuid& id)
{
    const auto currentMenu = menu();
    const auto actions = currentMenu->actions();
    const auto it = std::find_if(actions.begin(), actions.end(),
        [id](QAction* action)
        {
            const auto server = action->data().value<QnMediaServerResourcePtr>();
            return server && server->getId() == id;
        });

    return it == actions.end() ? nullptr : *it;
}
