// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class VmsRulesListHeaderWidget; }

namespace nx::vms::client::desktop {
namespace vms_rules {

class VmsRulesListHeaderWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    VmsRulesListHeaderWidget(QWidget* parent = nullptr);
    virtual ~VmsRulesListHeaderWidget() override;

private:
    QScopedPointer<Ui::VmsRulesListHeaderWidget> m_ui;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
