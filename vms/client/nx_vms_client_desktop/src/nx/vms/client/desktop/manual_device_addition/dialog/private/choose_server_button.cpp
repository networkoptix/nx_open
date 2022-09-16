// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "choose_server_button.h"

#include <QtWidgets/QMenu>

#include <nx/vms/client/desktop/style/skin.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/assert.h>

namespace {

QString getText(const QnMediaServerResourcePtr& server)
{
    return QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl);
}

QIcon getIcon(const QnMediaServerResourcePtr& server)
{
    // TODO: add icon for mismatchedCertificate status.
    return server->getStatus() == nx::vms::api::ResourceStatus::online
        ? qnSkin->icon("tree/server.png")
        : qnSkin->icon("tree/server_offline.png");
}

} // namespace

QnChooseServerButton::QnChooseServerButton(QWidget* parent):
    base_type(parent),
    m_serverStatus(this)
{
    setDisplayActionIcon(true);
}

bool QnChooseServerButton::setCurrentServer(const QnMediaServerResourcePtr& server)
{
    if (server && !actionForServer(server->getId()))
    {
        NX_ASSERT(false, "Can't find action for specified server");
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
        NX_ASSERT(false, "Server exists already");
        return;
    }

    addMenuItemForServer(server);
}

void QnChooseServerButton::removeServer(const QnUuid& id)
{
    if (const auto action = actionForServer(id))
        menu()->removeAction(action);
    else
        NX_ASSERT(false, "Server does not exist");
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

    connect(server.get(), &QnMediaServerResource::nameChanged, this,
        [action, server]() { action->setText(getText(server)); });
    connect(server.get(), &QnMediaServerResource::statusChanged, this,
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
