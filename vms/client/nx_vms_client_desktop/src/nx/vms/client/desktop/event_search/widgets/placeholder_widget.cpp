// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "placeholder_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/custom_style.h>

namespace nx::vms::client::desktop {

namespace {

constexpr auto kPlaceholderSpacing = 16;
constexpr auto kPlaceholderMargins = 24;
const auto kPlaceholderIconSize = QSize{64, 64};

} // namespace

PlaceholderWidget::PlaceholderWidget(QWidget* parent): QWidget{parent}
{
    auto layout = new QVBoxLayout;
    layout->setSpacing(kPlaceholderSpacing);
    layout->setContentsMargins(kPlaceholderMargins, 0, kPlaceholderMargins, 0);
    layout->addStretch(2);

    m_placeholderIcon = new QLabel;
    m_placeholderIcon->setFixedSize(kPlaceholderIconSize);
    layout->addWidget(m_placeholderIcon, 0, Qt::AlignHCenter);

    m_placeholderText = new QLabel;
    m_placeholderText->setWordWrap(true);
    m_placeholderText->setProperty(style::Properties::kDontPolishFontProperty, true);
    m_placeholderText->setFont(fontConfig()->large());
    m_placeholderText->setForegroundRole(QPalette::Mid);
    m_placeholderText->setAlignment(Qt::AlignCenter);
    m_placeholderText->setWordWrap(true);
    layout->addWidget(m_placeholderText);

    m_placeholderAction = new QPushButton;
    m_placeholderAction->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_placeholderAction->setVisible(false);
    connect(
        m_placeholderAction,
        &QPushButton::clicked,
        this,
        [this]
        {
            if (m_actionFunction)
                m_actionFunction();
        });
    layout->addWidget(m_placeholderAction, 0, Qt::AlignHCenter);

    layout->addStretch(3);

    setLayout(layout);
}

void PlaceholderWidget::setPixmap(const QPixmap& pixmap)
{
    m_placeholderIcon->setPixmap(pixmap);
}

void PlaceholderWidget::setText(const QString& text)
{
    m_placeholderText->setText(text);
}

void PlaceholderWidget::setAction(const QString& title, ActionFunction actionFunction)
{
    if (title.isEmpty() || !actionFunction)
    {
        m_placeholderAction->setVisible(false);
        return;
    }

    m_placeholderAction->setVisible(true);
    m_placeholderAction->setText(title);
    m_actionFunction = actionFunction;
}

PlaceholderWidget* PlaceholderWidget::create(
    const QPixmap& pixmap,
    const QString& text,
    QWidget* parent)
{
    return create(pixmap, text, {}, {}, parent);
}

PlaceholderWidget* PlaceholderWidget::create(
    const QPixmap& pixmap,
    const QString& text,
    const QString& action,
    ActionFunction actionFunction,
    QWidget* parent)
{
    auto widget = new PlaceholderWidget{parent};

    widget->setPixmap(pixmap);
    widget->setText(text);

    if (!action.isEmpty() && actionFunction)
        widget->setAction(action, actionFunction);

    return widget;
}

} // namespace nx::vms::client::desktop
