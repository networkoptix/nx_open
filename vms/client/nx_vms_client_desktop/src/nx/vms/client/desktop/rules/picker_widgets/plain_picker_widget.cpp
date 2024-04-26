// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plain_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace nx::vms::client::desktop::rules {

PlainPickerWidget::PlainPickerWidget(SystemContext* context, ParamsWidget* parent):
    PickerWidget{context, parent}
{
    auto mainLayout = new QHBoxLayout{this};
    mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    mainLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin,
        4,
        style::Metrics::kDefaultTopLevelMargin,
        4);

    m_label = new WidgetWithHint<QnElidedLabel>;
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setElideMode(Qt::ElideRight);
    m_label->setFixedHeight(28);
    m_label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    mainLayout->addWidget(m_label);
    mainLayout->setAlignment(m_label, Qt::AlignTop);

    auto contentLayout = new QVBoxLayout;
    m_contentWidget = new QWidget;
    contentLayout->addWidget(m_contentWidget);

    m_alertLabel = new QLabel;
    m_alertLabel->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    m_alertLabel->setVisible(false);
    m_alertLabel->setWordWrap(true);
    setWarningStyle(m_alertLabel);
    m_alertLabel->setTextFormat(Qt::TextFormat::RichText);
    contentLayout->addWidget(m_alertLabel);

    mainLayout->addLayout(contentLayout);

    mainLayout->setStretch(0, 1);
    mainLayout->setStretch(1, 5);
}

void PlainPickerWidget::setReadOnly(bool value)
{
    m_contentWidget->setEnabled(!value);
}

void PlainPickerWidget::setValidity(const vms::rules::ValidationResult& validationResult)
{
    m_alertLabel->setVisible(validationResult.validity != QValidator::State::Acceptable
        && !validationResult.description.isEmpty());

    if (!validationResult.description.isEmpty())
        m_alertLabel->setText(validationResult.description);
}

void PlainPickerWidget::onDescriptorSet()
{
    m_label->setText(m_fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
