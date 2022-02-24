// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ShowNotificationActionParamsWidget; }

namespace nx::vms::client::desktop {
namespace vms_rules {

/**
 * Show Notification Event parameters editor widget sketch.
 * @todo Should implement interface which allows:
 *     - Setup editor state from key/value storage
 *     - Get key/value storage bases on editor state
 * @todo Should be created by action/event parameter editors factory.
 */
class ShowNotificationActionParamsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ShowNotificationActionParamsWidget(QWidget* parent = nullptr);
    virtual ~ShowNotificationActionParamsWidget() override;

private:
    QScopedPointer<Ui::ShowNotificationActionParamsWidget> m_ui;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
