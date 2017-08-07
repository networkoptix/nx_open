#include "analytics_sdk_event_widget.h"
#include "ui_analytics_sdk_event_widget.h"

#include <QtGui/QDesktopServices>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/scoped_value_rollback.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

AnalyticsSdkEventWidget::AnalyticsSdkEventWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::AnalyticsSdkEventWidget)
{
    ui->setupUi(this);

    connect(ui->captionEdit,     &QLineEdit::textChanged, this, &AnalyticsSdkEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this, &AnalyticsSdkEventWidget::paramsChanged);

    const QString description = tr("Event will trigger only if Analytics Event meets all the above conditions. "
        "If a keyword field is empty, condition is always met. "
        "If not, condition is met if the corresponding field of Analytics Event contains any keyword.");

    ui->hintLabel->setText(description);
}

AnalyticsSdkEventWidget::~AnalyticsSdkEventWidget()
{
}

void AnalyticsSdkEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, after);
}

void AnalyticsSdkEventWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(Field::eventParams))
    {
        QString caption = model()->eventParams().caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = model()->eventParams().description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);
    }
}

void AnalyticsSdkEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    auto eventParams = model()->eventParams();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    model()->setEventParams(eventParams);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
