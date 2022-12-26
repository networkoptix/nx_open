// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "volume_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

VolumePickerWidget::VolumePickerWidget(SystemContext* context, QWidget* parent):
    PickerWidget(context, parent)
{
    auto contentLayout = new QHBoxLayout;

    contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    contentLayout->setObjectName("contentLayout");
    m_slider = new QSlider;
    m_slider->setOrientation(Qt::Horizontal);
    contentLayout->addWidget(m_slider);

    auto buttonLayout = new QHBoxLayout;

    buttonLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    m_pushButton = new QPushButton;
    m_pushButton->setText(CommonPickerWidgetStrings::testButtonDisplayText());
    buttonLayout->addWidget(m_pushButton);

    auto horizontalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonLayout->addItem(horizontalSpacer);

    contentLayout->addLayout(buttonLayout);

    m_contentWidget->setLayout(contentLayout);
}

} // namespace nx::vms::client::desktop::rules
