#pragma once

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QAction>
#include <QtWidgets/QPushButton>

#include <nx/client/desktop/common/utils/command_action.h>
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
    explicit ActionPushButton(const CommandActionPtr& action, QWidget* parent = nullptr);
    virtual ~ActionPushButton() override;

    CommandActionPtr action() const;
    void setAction(const CommandActionPtr& value);

private:
    void updateFromAction();

private:
    CommandActionPtr m_action;
    QScopedPointer<QnDisconnectHelper> m_actionConnections;
};

} // namespace desktop
} // namespace client
} // namespace nx
