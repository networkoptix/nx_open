// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QVariant>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {
namespace vms_rules {

class EventActionParamsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    EventActionParamsWidget(QWidget* parent = nullptr);

    virtual std::map<QString, QVariant> flatData() const = 0;
    virtual void setFlatData(const std::map<QString, QVariant>& flatData) = 0;
    virtual void setReadOnly(bool value) = 0;

protected:
    void setupLineEditsPlaceholderColor();
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
