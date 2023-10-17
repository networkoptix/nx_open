// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "titled_picker_widget.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

TitledPickerWidget::TitledPickerWidget(SystemContext* context, CommonParamsWidget* parent):
    PickerWidget{context, parent}
{
    auto mainLayout = new QVBoxLayout{this};
    mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    mainLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin,
        8,
        style::Metrics::kDefaultTopLevelMargin,
        8);

    {
        auto titleLayout = new QHBoxLayout;

        m_title = new QLabel;

        QFont font;
        font.setPixelSize(10);
        font.setWeight(QFont::Medium);
        m_title->setFont(font);

        setPaletteColor(m_title, QPalette::WindowText, core::colorTheme()->color("light10"));

        titleLayout->addWidget(m_title);

        titleLayout->addStretch();

        m_enabledSwitch = new SlideSwitch;
        m_enabledSwitch->setChecked(true);
        connect(
            m_enabledSwitch,
            &SlideSwitch::toggled,
            this,
            &TitledPickerWidget::onEnabledChanged);

        titleLayout->addWidget(m_enabledSwitch);

        mainLayout->addLayout(titleLayout);
    }

    {
        m_contentWidget = new QWidget;
        mainLayout->addWidget(m_contentWidget);
    }
}

void TitledPickerWidget::setCheckBoxEnabled(bool value)
{
    m_enabledSwitch->setVisible(value);
}

void TitledPickerWidget::setChecked(bool value)
{
    m_enabledSwitch->setChecked(value);
}

bool TitledPickerWidget::isChecked() const
{
    return m_enabledSwitch->isChecked();
}

void TitledPickerWidget::setReadOnly(bool value)
{
    setEnabled(!value);
}

void TitledPickerWidget::onEnabledChanged(bool isEnabled)
{
    m_contentWidget->setVisible(isEnabled);
}

void TitledPickerWidget::onDescriptorSet()
{
    m_title->setText(m_fieldDescriptor->displayName.toUpper());
}

} // namespace nx::vms::client::desktop::rules
