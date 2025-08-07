// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plain_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>

#include "private/multiline_elided_label.h"

namespace nx::vms::client::desktop::rules {

PlainPickerWidget::PlainPickerWidget(
    const QString& displayName,
    SystemContext* context,
    ParamsWidget* parent)
    :
    PickerWidget{context, parent}
{
    auto mainLayout = new QHBoxLayout{this};
    mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    mainLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin,
        4,
        style::Metrics::kDefaultTopLevelMargin,
        4);

    auto labelWithHintLayout = new QHBoxLayout;
    m_label = new MultilineElidedLabel;
    m_label->setFixedHeight(30);
    m_label->setText(displayName);
    labelWithHintLayout->addWidget(m_label);

    m_hintButton = new HintButton;
    m_hintButton->setVisible(false);
    labelWithHintLayout->addWidget(m_hintButton);
    labelWithHintLayout->setAlignment(m_hintButton, Qt::AlignCenter);

    mainLayout->addLayout(labelWithHintLayout);
    mainLayout->setAlignment(labelWithHintLayout, Qt::AlignTop);

    auto contentLayout = new QVBoxLayout;
    m_contentWidget = new QWidget;
    contentLayout->addWidget(m_contentWidget);

    m_alertLabel = new AlertLabel{this};
    m_alertLabel->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    m_alertLabel->setVisible(false);
    setWarningStyle(m_alertLabel);
    contentLayout->addWidget(m_alertLabel);

    mainLayout->addLayout(contentLayout);

    mainLayout->setStretch(0, 1);
    mainLayout->setStretch(1, 5);
}

void PlainPickerWidget::setReadOnly(bool value)
{
    m_contentWidget->setEnabled(!value);
}

void PlainPickerWidget::setDisplayName(const QString& displayName)
{
    m_label->setText(displayName);
}

void PlainPickerWidget::setValidity(const vms::rules::ValidationResult& validationResult)
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
