// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_headers_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>

#include "private/custom_http_headers_dialog.h"
#include "private/multiline_elided_label.h"

namespace nx::vms::client::desktop::rules {

HttpHeadersPickerWidget::HttpHeadersPickerWidget(
    vms::rules::HttpHeadersField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    PlainFieldPickerWidget<vms::rules::HttpHeadersField>(field, context, parent)
{
    auto contentLayout = new QHBoxLayout{m_contentWidget};

    m_label->setText({});

    m_button = new QPushButton;
    m_button->setProperty(
        style::Properties::kPushButtonMargin, style::Metrics::kStandardPadding);
    contentLayout->addWidget(m_button);

    connect(
        m_button,
        &QPushButton::clicked,
        this,
        &HttpHeadersPickerWidget::onButtonClicked);
}

void HttpHeadersPickerWidget::updateUi()
{
    PlainFieldPickerWidget<vms::rules::HttpHeadersField>::updateUi();

    m_button->setText(m_field->value().empty()
        ? tr("No custom headers")
        : tr("%n custom headers", "%n is a number of custom headers", m_field->value().size()));
}

void HttpHeadersPickerWidget::onButtonClicked()
{
    auto dialog = new CustomHttpHeadersDialog{m_field->value(), this};
    connect(
        dialog,
        &CustomHttpHeadersDialog::done,
        this,
        [this, dialog](bool accepted)
        {
            if (accepted)
                m_field->setValue(dialog->headers());

            dialog->deleteLater();
        });

    dialog->open();
}

} // namespace nx::vms::client::desktop::rules
