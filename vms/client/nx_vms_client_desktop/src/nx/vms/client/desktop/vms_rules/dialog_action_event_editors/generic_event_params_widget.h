// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "event_action_params_widget.h"

namespace Ui { class GenericEventParamsWidget; }

namespace nx::vms::client::desktop {
namespace vms_rules {

class GenericEventParamsWidget: public EventActionParamsWidget
{
    Q_OBJECT
    using base_type = EventActionParamsWidget;

public:
    GenericEventParamsWidget(QWidget* parent = nullptr);
    virtual ~GenericEventParamsWidget() override;

    virtual std::map<QString, QVariant> flatData() const override;
    virtual void setFlatData(const std::map<QString, QVariant>& flatData) override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::GenericEventParamsWidget> m_ui;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
