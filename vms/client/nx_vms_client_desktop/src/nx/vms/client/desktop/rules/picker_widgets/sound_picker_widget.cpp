// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sound_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

SoundPickerWidget::SoundPickerWidget(SystemContext* context, QWidget* parent):
    PickerWidget(context, parent)
{
    auto contentLayout = new QHBoxLayout;

    contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    m_comboBox = new QComboBox;
    contentLayout->addWidget(m_comboBox);

    auto buttonLayout = new QHBoxLayout;
    m_pushButton = new QPushButton;
    m_pushButton->setText(CommonPickerWidgetStrings::testButtonDisplayText());
    buttonLayout->addWidget(m_pushButton);

    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonLayout->addItem(horizontalSpacer);

    contentLayout->addLayout(buttonLayout);

    m_contentWidget->setLayout(contentLayout);
}

} // namespace nx::vms::client::desktop::rules
