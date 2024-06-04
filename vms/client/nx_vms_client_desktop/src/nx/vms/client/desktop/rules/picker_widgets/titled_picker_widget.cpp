// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "titled_picker_widget.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

TitledPickerWidget::TitledPickerWidget(
    const QString& displayName,
    SystemContext* context,
    ParamsWidget* parent)
    :
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
        m_title->setText(displayName.toUpper());

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

    {
        m_alertLabel = new AlertLabel{this};
        m_alertLabel->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        m_alertLabel->setVisible(false);
        setWarningStyle(m_alertLabel);
        mainLayout->addWidget(m_alertLabel);
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

void TitledPickerWidget::setValidity(const vms::rules::ValidationResult& validationResult)
{
    if (validationResult.validity == QValidator::State::Acceptable
        || validationResult.description.isEmpty())
    {
        m_alertLabel->setVisible(false);
        m_alertLabel->setText({});
        return;
    }

    m_alertLabel->setType(validationResult.validity == QValidator::State::Intermediate
        ? AlertLabel::Type::warning
        : AlertLabel::Type::error);
    m_alertLabel->setText(validationResult.description);
    m_alertLabel->setVisible(true);
}

} // namespace nx::vms::client::desktop::rules
