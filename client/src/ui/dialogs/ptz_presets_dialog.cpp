#include "ptz_presets_dialog.h"
#include "ui_ptz_presets_dialog.h"

#include <QtWidgets/QPushButton>
#include <QtGui/QStandardItem>

#include <api/kvpair_usage_helper.h>

#include <core/resource/camera_resource.h>

#include <core/ptz/abstract_ptz_controller.h>

#include <ui/workbench/workbench_context.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/models/ptz_preset_list_model.h>
#include <ui/delegates/ptz_preset_hotkey_item_delegate.h>

QnPtzPresetsDialog::QnPtzPresetsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PtzPresetsDialog),
    m_model(new QnPtzPresetListModel(this)),
    m_helper(new QnStringKvPairUsageHelper(QnResourcePtr(), lit("ptz_hotkeys"), QString(), this))
{
    ui->setupUi(this);

    m_removeButton = new QPushButton(tr("Remove"));
    m_activateButton = new QPushButton(tr("Activate"));

    connect(m_helper, SIGNAL(valueChanged(QString)), m_model, SLOT(setSerializedHotkeys(QString)));

    ui->treeView->setModel(m_model);
    ui->treeView->setItemDelegateForColumn(m_model->column(QnPtzPresetListModel::HotkeyColumn), new QnPtzPresetHotkeyItemDelegate(this));
    ui->buttonBox->addButton(m_removeButton, QDialogButtonBox::HelpRole);
    ui->buttonBox->addButton(m_activateButton, QDialogButtonBox::HelpRole);

    connect(m_removeButton,                 SIGNAL(clicked()),                                                          this,   SLOT(at_removeButton_clicked()));
    connect(m_activateButton,               SIGNAL(clicked()),                                                          this,   SLOT(at_activateButton_clicked()));
    connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),   this,   SLOT(updateRemoveButtonEnabled()));
    connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),   this,   SLOT(updateActivateButtonEnabled()));
    connect(ui->treeView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),           this,   SLOT(updateActivateButtonEnabled()));

    updateFromResource();
}

QnPtzPresetsDialog::~QnPtzPresetsDialog() {
    return;
}

void QnPtzPresetsDialog::setPtzController(const QnPtzControllerPtr &controller) {
    if(m_controller == controller)
        return;

    if(m_controller && m_controller->resource())
        disconnect(m_controller->resource(), NULL, this, NULL);

    m_controller = controller;

    if(m_controller && m_controller->resource()) {
        connect(m_controller->resource(), SIGNAL(nameChanged(const QnResourcePtr &)), this, SLOT(updateLabel()));
        m_helper->setResource(m_controller->resource());
    } else {
        m_helper->setResource(QnResourcePtr());
    }
    updateFromResource();
}

void QnPtzPresetsDialog::accept() {
    submitToResource();
    base_type::accept();
}

void QnPtzPresetsDialog::updateFromResource() {
    updateLabel();
    updateModel();
    updateRemoveButtonEnabled();
    updateActivateButtonEnabled();
}

bool findPresetById(const QnPtzPresetList &list, const QString &id, QnPtzPreset &result) {
    foreach (const QnPtzPreset &preset, list) {
        if (preset.id != id)
            continue;
        result = preset;
        return true;
    }
    return false;
}

void QnPtzPresetsDialog::submitToResource() {
    m_helper->setValue(m_model->serializedHotkeys());

    QnPtzPresetList presets;

    if(!m_controller || !m_controller->getPresets(&presets))
        return;

    foreach (QnPtzPreset preset, presets) {
        QnPtzPreset updated;
        if (!findPresetById(m_model->presets(), preset.id, updated))
            m_controller->removePreset(preset.id);
        else if (preset.name != updated.name)
            m_controller->updatePreset(updated);
    }
}

void QnPtzPresetsDialog::updateLabel() {
    ui->topLabel->setText((m_controller && m_controller->resource())
                          ? tr("PTZ presets for camera %1:").arg(getResourceName(m_controller->resource()))
                          : QString());
}

void QnPtzPresetsDialog::updateModel() {
    QnPtzPresetList presets;
    if (m_controller && m_controller->getPresets(&presets))
        m_model->setPresets(presets);
    else
        m_model->setPresets(QnPtzPresetList());
}

void QnPtzPresetsDialog::updateRemoveButtonEnabled() {
    m_removeButton->setEnabled(m_controller && ui->treeView->selectionModel()->selectedRows().size() > 0);
}

void QnPtzPresetsDialog::updateActivateButtonEnabled() {
    m_activateButton->setEnabled(m_controller && ui->treeView->currentIndex().isValid() && ui->treeView->selectionModel()->selectedRows().size() <= 1);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnPtzPresetsDialog::at_removeButton_clicked() {
    QList<QPersistentModelIndex> indices;
    foreach(const QModelIndex &index, ui->treeView->selectionModel()->selectedRows())
        indices.push_back(index);

    foreach(const QPersistentModelIndex &index, indices)
        m_model->removeRow(index.row(), index.parent());
}

void QnPtzPresetsDialog::at_activateButton_clicked() {
    QnPtzPreset preset = ui->treeView->currentIndex().data(Qn::PtzPresetRole).value<QnPtzPreset>();
    m_controller->activatePreset(preset.id);
}



