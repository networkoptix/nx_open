// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_business_event_widget.h"
#include "ui_custom_business_event_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QDesktopServices>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/common/html/html.h>

using namespace nx::vms::client::desktop;

namespace {

static const QString kDocumentationScheme = nx::network::http::kSecureUrlSchemeName;
static const QString kApiDocFragment = "/api-tool/api-createevent-get?version=current%20version";

} // namespace

QnCustomBusinessEventWidget::QnCustomBusinessEventWidget(
    SystemContext* systemContext,
    QWidget* parent):
    base_type(systemContext, parent),
    ui(new Ui::CustomBusinessEventWidget)
{
    using namespace nx::vms::common;

    ui->setupUi(this);

    connect(ui->deviceNameEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->captionEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);

    connect(ui->omitLogging, SIGNAL(toggled(bool)), this, SLOT(setOmitLogging(bool)));

    ui->sourceLabel->addHintLine(tr("Event will trigger only if there are matches in the source with any of the entered keywords."));
    ui->sourceLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    setHelpTopic(ui->sourceLabel, HelpTopic::Id::EventsActions_Generic);

    ui->captionLabel->addHintLine(tr("Event will trigger only if there are matches in the caption with any of the entered keywords."));
    ui->captionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    setHelpTopic(ui->captionLabel, HelpTopic::Id::EventsActions_Generic);

    ui->descriptionLabel->addHintLine(tr("Event will trigger only if there are matches in the description with any of the entered keywords."));
    ui->descriptionLabel->addHintLine(tr("If the field is empty, event will always trigger."));
    setHelpTopic(ui->descriptionLabel, HelpTopic::Id::EventsActions_Generic);

    if (auto server = currentServer())
    {
        const auto url = nx::network::url::Builder()
            .setScheme(kDocumentationScheme)
            .setEndpoint(server->getPrimaryAddress())
            .setFragment(kApiDocFragment)
            .toUrl();

        const QString documentationHint = tr("To generate Generic Event, please refer to %1.")
            .arg(html::link(tr("Server API"), url));

        ui->hintLabel->setTextFormat(Qt::RichText);
        ui->hintLabel->setOpenExternalLinks(true);
        ui->hintLabel->setText(html::kHorizontalLine + documentationHint);
    }
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

    QScopedValueRollback<bool> guard(m_updating, true);

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
