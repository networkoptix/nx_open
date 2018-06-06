#include "custom_business_event_widget.h"
#include "ui_custom_business_event_widget.h"

#include <QtGui/QDesktopServices>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <utils/common/scoped_value_rollback.h>

namespace {

    static const QString kDocumentationScheme = lit("http");
    static const QString kApiDocPath = lit("/static/api.xml");
    static const QString kApiDocFragment = lit("group_Server_API_method_createEvent");

} // namespace

QnCustomBusinessEventWidget::QnCustomBusinessEventWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CustomBusinessEventWidget)
{
    ui->setupUi(this);

    connect(ui->deviceNameEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->captionEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);

    ui->sourceLabel->addHintLine(tr("Event will trigger only if there are matches in the source with any of the entered keywords."));
    ui->sourceLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    setHelpTopic(ui->sourceLabel, Qn::EventsActions_Generic_Help);

    ui->captionLabel->addHintLine(tr("Event will trigger only if there are matches in the caption with any of the entered keywords."));
    ui->captionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    setHelpTopic(ui->captionLabel, Qn::EventsActions_Generic_Help);

    ui->descriptionLabel->addHintLine(tr("Event will trigger only if there are matches in the description with any of the entered keywords."));
    ui->descriptionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    setHelpTopic(ui->descriptionLabel, Qn::EventsActions_Generic_Help);
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
