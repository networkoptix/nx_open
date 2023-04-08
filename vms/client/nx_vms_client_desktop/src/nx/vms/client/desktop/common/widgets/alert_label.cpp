// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "alert_label.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <ui/common/palette.h>
#include <ui/widgets/word_wrapped_label.h>

namespace nx::vms::client::desktop {

AlertLabel::AlertLabel(QWidget* parent):
    base_class(parent),
    m_alertText(new QnWordWrappedLabel(this))
{
    auto alertIcon = new QLabel(this);
    alertIcon->setPixmap(qnSkin->pixmap("tree/warning_yellow.svg"));
    alertIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    setPaletteColor(m_alertText, QPalette::WindowText, core::colorTheme()->color("yellow_d1"));
    setPaletteColor(m_alertText, QPalette::Link, core::colorTheme()->color("yellow_d1"));
    setPaletteColor(m_alertText, QPalette::LinkVisited, core::colorTheme()->color("yellow_core"));
    connect(m_alertText, &QnWordWrappedLabel::linkActivated, this, &AlertLabel::linkActivated);

    auto layout = new QHBoxLayout(this);
    layout->addWidget(alertIcon);
    layout->setAlignment(alertIcon, Qt::AlignTop);
    layout->addWidget(m_alertText);
}

void AlertLabel::setText(const QString& text)
{
    m_alertText->setText(text);
}

} // namespace nx::vms::client::desktop
