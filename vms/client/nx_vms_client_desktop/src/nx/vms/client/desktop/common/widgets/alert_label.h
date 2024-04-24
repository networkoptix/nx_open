// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

class QnWordWrappedLabel;
class QLabel;

namespace nx::vms::client::desktop {

class AlertLabel: public QWidget
{
    Q_OBJECT
    using base_class = QWidget;

public:
    enum Type {
        info,
        warning
    };

public:
    AlertLabel(QWidget* parent);

    void setText(const QString& text);
    void setType(Type type);

signals:
    void linkActivated(const QString& link);

private:
    QnWordWrappedLabel* m_alertText = nullptr;
    QLabel* m_alertIcon = nullptr;
    Type m_type = warning;
};

} // namespace nx::vms::client::desktop
