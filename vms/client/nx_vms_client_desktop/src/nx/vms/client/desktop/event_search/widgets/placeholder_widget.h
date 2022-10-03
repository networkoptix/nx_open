// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

class PlaceholderWidget: public QWidget
{
    Q_OBJECT

public:
    PlaceholderWidget(QWidget* parent = nullptr);

    void setPixmap(const QPixmap& pixmap);
    void setText(const QString& text);
    using ActionFunction = std::function<void()>;
    void setAction(const QString& text, ActionFunction function);

    static PlaceholderWidget* create(
        const QPixmap& pixmap,
        const QString& text,
        QWidget* parent);

    static PlaceholderWidget* create(
        const QPixmap& pixmap,
        const QString& text,
        const QString& action,
        ActionFunction actionFunction,
        QWidget* parent);

private:
    QLabel* m_placeholderIcon;
    QLabel* m_placeholderText;
    QPushButton* m_placeholderAction;
    ActionFunction m_actionFunction;
};

} // namespace nx::vms::client::desktop
