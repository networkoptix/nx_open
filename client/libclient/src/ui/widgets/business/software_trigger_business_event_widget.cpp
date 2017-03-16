#include "software_trigger_business_event_widget.h"
#include "ui_software_trigger_business_event_widget.h"

#include <QtCore/QScopedValueRollback>

#include <business/business_strings_helper.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <ui/common/aligner.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/graphics/items/overlays/software_trigger_icons.h>
#include <ui/workaround/widgets_signals_workaround.h>


QnSoftwareTriggerBusinessEventWidget::QnSoftwareTriggerBusinessEventWidget(QWidget* parent) :
    base_type(parent),
    ui(new Ui::SoftwareTriggerBusinessEventWidget)
{
    ui->setupUi(this);

    ui->usersButton->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->triggerIdLineEdit->setPlaceholderText(QnBusinessStringsHelper::defaultSoftwareTriggerName());

    connect(ui->triggerIdLineEdit, &QLineEdit::textChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->iconComboBox, QnComboboxCurrentIndexChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
    connect(ui->usersButton, &QPushButton::clicked, this,
        &QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked);

    ui->iconComboBox->setIcons(
        QnSoftwareTriggerIcons::iconsPath(),
        QnSoftwareTriggerIcons::iconNames());

    auto aligner = new QnAligner(this);
    aligner->addWidget(ui->nameLabel);
    aligner->addWidget(ui->iconLabel);
    aligner->addWidget(ui->usersLabel);
}

QnSoftwareTriggerBusinessEventWidget::~QnSoftwareTriggerBusinessEventWidget()
{
}

void QnSoftwareTriggerBusinessEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->usersButton);
    setTabOrder(ui->iconComboBox, after);
}

void QnSoftwareTriggerBusinessEventWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(QnBusiness::EventParamsField))
    {
        const auto params = model()->eventParams();
        const auto text = params.inputPortId.trimmed();
        ui->triggerIdLineEdit->setText(text);
        ui->iconComboBox->setCurrentIcon(params.caption);

        if (params.metadata.instigators.empty())
        {
            ui->usersButton->selectAll();
        }
        else
        {
            auto users = qnResPool->getResources<QnUserResource>(params.metadata.instigators);
            ui->usersButton->selectUsers(users);
        }
    }
}

void QnSoftwareTriggerBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto eventParams = model()->eventParams();

    eventParams.inputPortId = ui->triggerIdLineEdit->text().trimmed();
    eventParams.caption = ui->iconComboBox->currentIcon();

    model()->setEventParams(eventParams);
}

void QnSoftwareTriggerBusinessEventWidget::at_usersButton_clicked()
{
    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::users, this);
    QSet<QnUuid> selected;

    auto params = model()->eventParams();

    for (const auto& id: params.metadata.instigators)
        selected << id;

    dialog.setSelectedResources(selected);

    if (dialog.exec() != QDialog::Accepted)
        return;

    selected = dialog.selectedResources();

    params.metadata.instigators = decltype(params.metadata.instigators)(
        selected.constBegin(),
        selected.constEnd());

    model()->setEventParams(params);
}
