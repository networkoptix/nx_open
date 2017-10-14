#pragma once

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QAction>
#include <QtWidgets/QPushButton>

#include <nx/utils/disconnect_helper.h>

namespace nx {
namespace client {
namespace desktop {

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
    QScopedPointer<QnDisconnectHelper> m_actionConnections;
};

} // namespace desktop
} // namespace client
} // namespace nx
