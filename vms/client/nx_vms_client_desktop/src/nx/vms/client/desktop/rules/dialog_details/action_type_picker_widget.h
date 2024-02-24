// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace Ui { class ActionTypePickerWidget; }

namespace nx::vms::rules { class Engine; }

namespace nx::vms::client::desktop::rules {

class ActionTypePickerWidget:
    public QWidget,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit ActionTypePickerWidget(SystemContext* context, QWidget* parent = nullptr);
    virtual ~ActionTypePickerWidget() override;

    QString actionType() const;
    void setActionType(const QString& actionType);

signals:
    void actionTypePicked(const QString& actionType);

private:
    QScopedPointer<Ui::ActionTypePickerWidget> ui;
};

} // namespace nx::vms::client::desktop::rules
