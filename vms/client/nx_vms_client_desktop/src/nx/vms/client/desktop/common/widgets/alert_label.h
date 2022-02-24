// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QnWordWrappedLabel;

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class AlertLabel: public QWidget
{
    Q_OBJECT
    using base_class = QWidget;

public:
    AlertLabel(QWidget* parent);

    void setText(const QString& text);

signals:
    void linkActivated(const QString& link);

private:
    QnWordWrappedLabel* m_alertText = nullptr;
};

} // namespace nx::vms::client::desktop
