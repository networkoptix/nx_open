// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_widget.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>

namespace nx::vms::client::desktop {

Thumbnail::Thumbnail(
    const QString& text,
    const QString& iconPath,
    const core::SvgIconColorer::ThemeSubstitutions& theme,
    QWidget* parent)
    :
    QWidget{parent}
{
    setupUi();

    m_iconWidget->setPixmap(theme.isEmpty()
        ? qnSkin->pixmap(iconPath)
        : qnSkin->icon(iconPath, theme).pixmap(core::kIconSize * qApp->devicePixelRatio()));
    m_textWidget->setText(text);
}

Thumbnail::Thumbnail(Qn::ResourceStatusOverlay overlay, QWidget* parent): QWidget{parent}
{
    setupUi();

    m_iconWidget->setPixmap(QnStatusOverlayController::statusIcon(overlay));
    m_textWidget->setText(QnStatusOverlayController::caption(overlay));
}

void Thumbnail::updateToSize(const QSize& size)
{
    const auto tenPercentsOfWidth = qRound(size.width() * 0.1);

    // Status icon size should be approximately 10% of the widget width.
    m_iconWidget->setFixedSize({tenPercentsOfWidth, tenPercentsOfWidth});

    // Text font should be approximately 10% of the widget width.
    auto font = m_textWidget->font();
    font.setPixelSize(tenPercentsOfWidth);
    m_textWidget->setFont(font);

    // Spacing should be three times smaller than content size.
    layout()->setSpacing(tenPercentsOfWidth / 3);

    resize(size);
}

void Thumbnail::setupUi()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addStretch();

    m_iconWidget = new QLabel;
    m_iconWidget->setScaledContents(true);
    m_iconWidget->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconWidget, 0, Qt::AlignCenter);

    m_textWidget = new QLabel;
    m_textWidget->setWordWrap(true);
    m_textWidget->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_textWidget, 0, Qt::AlignCenter);

    layout->addStretch();
}

} // namespace nx::vms::client::desktop
