// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plain_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>

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

    m_label = new WidgetWithHint<QnWordWrappedLabel>;
    m_label->label()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setFixedHeight(28);
    m_label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    m_label->setText(displayName);
    mainLayout->addWidget(m_label);
    mainLayout->setAlignment(m_label, Qt::AlignTop);

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
