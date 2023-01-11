// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ActionTypePickerWidget; }

namespace nx::vms::rules { class Engine; }

namespace nx::vms::client::desktop::rules {

class ActionTypePickerWidget: public QWidget
{
    Q_OBJECT

public:
    ActionTypePickerWidget(QWidget* parent = nullptr);
    virtual ~ActionTypePickerWidget() override;

    void init(nx::vms::rules::Engine* engine);

    QString actionType() const;
    void setActionType(const QString& actionType);

signals:
    void actionTypePicked(const QString& actionType);

private:
    QScopedPointer<Ui::ActionTypePickerWidget> ui;
};

} // namespace nx::vms::client::desktop::rules
