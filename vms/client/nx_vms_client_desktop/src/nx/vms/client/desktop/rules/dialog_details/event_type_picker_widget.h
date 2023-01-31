// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/api/rules/event_info.h>

namespace Ui { class EventTypePickerWidget; }

namespace nx::vms::rules { class Engine; }

namespace nx::vms::client::desktop::rules {

class EventTypePickerWidget: public QWidget
{
    Q_OBJECT

public:
    EventTypePickerWidget(QWidget* parent = nullptr);
    virtual ~EventTypePickerWidget() override;

    void init(const nx::vms::rules::Engine* engine);

    QString eventType() const;
    void setEventType(const QString& eventType);

signals:
    void eventTypePicked(const QString& eventType);

private:
    QScopedPointer<Ui::EventTypePickerWidget> ui;
};

} // namespace nx::vms::client::desktop::rules
