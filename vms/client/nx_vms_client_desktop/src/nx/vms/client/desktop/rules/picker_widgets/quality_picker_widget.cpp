// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quality_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

namespace nx::vms::client::desktop::rules {

QualityPickerWidget::QualityPickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent)
{
    auto contentLayout = new QHBoxLayout;
    m_comboBox = new QComboBox;
    m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    contentLayout->addWidget(m_comboBox);

    m_contentWidget->setLayout(contentLayout);
}

} // namespace nx::vms::client::desktop::rules
