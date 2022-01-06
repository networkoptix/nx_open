// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtGui/QAction>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

class ActionPushButton: public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit ActionPushButton(QWidget* parent = nullptr);
    explicit ActionPushButton(QAction* action, QWidget* parent = nullptr);
    virtual ~ActionPushButton() override;

    QAction* action() const;
    void setAction(QAction* value);

private:
    void updateFromAction();

private:
    QPointer<QAction> m_action;
    nx::utils::ScopedConnections m_actionConnections;
};

} // namespace nx::vms::client::desktop
