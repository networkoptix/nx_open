#include "custom_business_event_widget.h"
#include "ui_custom_business_event_widget.h"

#include <QtGui/QDesktopServices>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/scoped_value_rollback.h>

namespace {
const QString kApiDocPath = lit("/static/api.xml");
const QString kApiDocFragment = lit("group_Server_API_method_createEvent");
} // namespace

QnCustomBusinessEventWidget::QnCustomBusinessEventWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CustomBusinessEventWidget)
{
    ui->setupUi(this);

    connect(ui->deviceNameEdit,  &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->captionEdit,     &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);

    const QString description = tr("Event will trigger only if Generic Event meets all the above conditions. "
        "If a keyword field is empty, condition is always met. "
        "If not, condition is met if the corresponding field of Generic Event contains any keyword.");

    const QString linkText = tr("Server API");
    const QString link = lit("<a href=\"api\">%1</a>").arg(linkText);
    const QString documentationHint = tr("To generate Generic Event, please refer to %1.").arg(link);

    ui->hintLabel->setTextFormat(Qt::RichText);
    ui->hintLabel->setText(lit("%1<hr/>%2").arg(description, documentationHint));
    connect(ui->hintLabel,  &QnWordWrappedLabel::linkActivated, this,
        [this]
        {
            auto server = commonModule()->currentServer();
            if (!server)
                return;

            nx::utils::Url targetUrl(server->getApiUrl());
            targetUrl.setPath(kApiDocPath);
            targetUrl.setFragment(kApiDocFragment);
            QDesktopServices::openUrl(targetUrl.toQUrl());
        });
}

QnCustomBusinessEventWidget::~QnCustomBusinessEventWidget()
{
}

void QnCustomBusinessEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->deviceNameEdit);
    setTabOrder(ui->deviceNameEdit, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, after);
}

void QnCustomBusinessEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(Field::eventResources))
    {
        // TODO: #rvasilenko nothing to do so far. waiting for POS integration
    }

    if (fields.testFlag(Field::eventParams))
    {
        QString resName = model()->eventParams().resourceName;
        if (ui->deviceNameEdit->text() != resName)
            ui->deviceNameEdit->setText(resName);

        QString caption = model()->eventParams().caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = model()->eventParams().description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);
    }
}

void QnCustomBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    auto eventParams = model()->eventParams();
    eventParams.resourceName = ui->deviceNameEdit->text();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    model()->setEventParams(eventParams);
}
