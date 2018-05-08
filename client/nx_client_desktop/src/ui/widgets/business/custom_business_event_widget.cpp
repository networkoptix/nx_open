#include "custom_business_event_widget.h"
#include "ui_custom_business_event_widget.h"

#include <QtGui/QDesktopServices>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/help/help_topics.h>
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

    connect(ui->omitLogging, SIGNAL(toggled(bool)), this, SLOT(setOmitLogging(bool)));

    ui->sourceLabelHint->addHintLine(tr("Event will trigger only if there are matches in caption with any of entered keywords."));
    ui->sourceLabelHint->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->sourceLabelHint->setHelpTopic(Qn::EventsActions_Generic_Help);

    ui->captionLabelHint->addHintLine(tr("Event will trigger only if there are matches in caption with any of entered keywords."));
    ui->captionLabelHint->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->captionLabelHint->setHelpTopic(Qn::EventsActions_Generic_Help);

    ui->descriptionLabelHint->addHintLine(tr("Event will trigger only if there are matches in caption with any of entered keywords."));
    ui->descriptionLabelHint->addHintLine(tr("If the field is empty, event will always trigger."));
    ui->descriptionLabelHint->setHelpTopic(Qn::EventsActions_Generic_Help);
}

QnCustomBusinessEventWidget::~QnCustomBusinessEventWidget()
{
}

void QnCustomBusinessEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->deviceNameEdit);
    setTabOrder(ui->deviceNameEdit, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, ui->omitLogging);
    setTabOrder(ui->omitLogging, after);
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
        const nx::vms::event::EventParameters & params = model()->eventParams();
        QString resName = params.resourceName;
        if (ui->deviceNameEdit->text() != resName)
            ui->deviceNameEdit->setText(resName);

        QString caption = params.caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = params.description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);

        bool omitLogging = params.omitDbLogging;
        if (ui->omitLogging->isChecked() != omitLogging)
            ui->omitLogging->setChecked(omitLogging);
    }
}

void QnCustomBusinessEventWidget::setOmitLogging(bool state)
{
    emit paramsChanged();
}

void QnCustomBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    auto eventParams = model()->eventParams();
    eventParams.resourceName = ui->deviceNameEdit->text();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    eventParams.omitDbLogging = ui->omitLogging->isChecked();
    model()->setEventParams(eventParams);
}
