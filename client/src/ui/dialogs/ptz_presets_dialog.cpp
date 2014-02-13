#include "ptz_presets_dialog.h"
#include "ui_ptz_presets_dialog.h"

#include <QtGui/QStandardItem>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QMessageBox>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>

#include <ui/models/ptz_preset_list_model.h>
#include <ui/delegates/ptz_preset_hotkey_item_delegate.h>
#include <ui/style/skin.h>

QnPtzPresetsDialog::QnPtzPresetsDialog(const QnPtzControllerPtr &controller, QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(controller, parent, windowFlags),
    ui(new Ui::PtzPresetsDialog),
    m_model(new QnPtzPresetListModel(this))
{
    ui->setupUi(this);

    m_removeButton = new QPushButton(tr("Remove"));
//    m_removeButton->setIcon(qnSkin->icon("buttons/remove.png"));

    m_activateButton = new QPushButton(tr("Activate"));

    ui->treeView->setModel(m_model);
    ui->treeView->setItemDelegateForColumn(m_model->column(QnPtzPresetListModel::HotkeyColumn), new QnPtzPresetHotkeyItemDelegate(this));
    ui->buttonBox->addButton(m_removeButton, QDialogButtonBox::HelpRole);
    ui->buttonBox->addButton(m_activateButton, QDialogButtonBox::HelpRole);

    connect(m_removeButton,                 SIGNAL(clicked()),                                                          this,   SLOT(at_removeButton_clicked()));
    connect(m_activateButton,               SIGNAL(clicked()),                                                          this,   SLOT(at_activateButton_clicked()));
    connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),   this,   SLOT(updateRemoveButtonEnabled()));
    connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),   this,   SLOT(updateActivateButtonEnabled()));
    connect(ui->treeView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),           this,   SLOT(updateActivateButtonEnabled()));
}

QnPtzPresetsDialog::~QnPtzPresetsDialog() {
}

void QnPtzPresetsDialog::loadData(const QnPtzData &data) {
    m_oldPresets = data.presets;
    m_model->setPresets(data.presets);
    updateRemoveButtonEnabled();
    updateActivateButtonEnabled();
}

bool QnPtzPresetsDialog::savePresets() {
    auto findPresetById = [](const QnPtzPresetList &list, const QString &id, QnPtzPreset &result) {
        foreach (const QnPtzPreset &preset, list) {
            if (preset.id != id)
                continue;
            result = preset;
            return true;
        }
        return false;
    };

    bool result = true;
    // update or remove existing presets
    foreach (QnPtzPreset preset, m_oldPresets) {
        QnPtzPreset updated;
        if (!findPresetById(m_model->presets(), preset.id, updated))
            result &= removePreset(preset.id);
        else if (preset != updated)
            result &= updatePreset(updated);
    }

    return result;
}

void QnPtzPresetsDialog::saveData() {
    savePresets();

    if (m_hotkeysDelegate)
        m_hotkeysDelegate->updateHotkeys(m_model->hotkeys());
    return;
}

Qn::PtzDataFields QnPtzPresetsDialog::requiredFields() const {
    return Qn::PresetsPtzField;
}

QnAbstractPtzHotkeyDelegate* QnPtzPresetsDialog::hotkeysDelegate() const {
    return m_hotkeysDelegate;
}

void QnPtzPresetsDialog::setHotkeysDelegate(QnAbstractPtzHotkeyDelegate *delegate) {
    m_hotkeysDelegate = delegate;
    if (delegate)
        m_model->setHotkeys(delegate->hotkeys());
}


void QnPtzPresetsDialog::updateRemoveButtonEnabled() {
    m_removeButton->setEnabled(ui->treeView->selectionModel()->selectedRows().size() > 0);
}

void QnPtzPresetsDialog::updateActivateButtonEnabled() {
    m_activateButton->setEnabled(ui->treeView->currentIndex().isValid() && ui->treeView->selectionModel()->selectedRows().size() <= 1);
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
    activatePreset(preset.id, 1.0);
}

