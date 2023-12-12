// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/widgets/scalable_image_widget.h>
#include <ui/widgets/word_wrapped_label.h>

#include "no_permissions_overlay_widget.h"

namespace nx::vms::client::desktop {

NoPermissionsOverlayWidget::NoPermissionsOverlayWidget(QWidget* parent):
    base_type(parent)
{
    setLayout(new QVBoxLayout(this));
    layout()->setAlignment(Qt::AlignCenter);

    auto placeholder = new QWidget(this);
    placeholder->setFixedSize(250, 140);
    layout()->addWidget(placeholder);

    auto layout = new QVBoxLayout(placeholder);
    placeholder->setLayout(layout);

    auto image = new ScalableImageWidget(":/skin/placeholders/no_camera_settings_access.svg");
    image->setFixedSize(64, 64);
    layout->addWidget(image);
    layout->setAlignment(image, Qt::AlignHCenter);

    auto labelNoAccess = new QLabel("No access");
    auto font = labelNoAccess->font();
    font.setPixelSize(16);
    font.setWeight(QFont::Medium);
    labelNoAccess->setFont(font);
    layout->addWidget(labelNoAccess);
    layout->setAlignment(labelNoAccess, Qt::AlignHCenter);

    m_infoLabel = new QnWordWrappedLabel();
    m_infoLabel->label()->setAlignment(Qt::AlignHCenter);
    font.setPixelSize(14);
    font.setWeight(QFont::Normal);
    m_infoLabel->label()->setFont(font);
    layout->addWidget(m_infoLabel);
    layout->setAlignment(m_infoLabel, Qt::AlignHCenter);

    setCameraCount(1);
}

void NoPermissionsOverlayWidget::setCameraCount(int value)
{
    if (value == 1)
        m_infoLabel->setText(tr("You do not have permission to edit settings of this camera"));
    else
        m_infoLabel->setText(tr("You do not have permission to edit settings of some cameras"));
}

void NoPermissionsOverlayWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), core::colorTheme()->color("dark7"));
}

} // namespace nx::vms::client::desktop
