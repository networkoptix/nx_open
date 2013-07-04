#include "ptz_presets_dialog.h"
#include "ui_ptz_presets_dialog.h"

#include <QtGui/QPushButton>
#include <QStandardItem>

#include <core/resource/camera_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/actions/action_manager.h>
#include <ui/models/ptz_preset_list_model.h>
#include <ui/delegates/ptz_preset_hotkey_item_delegate.h>


QnPtzPresetsDialog::QnPtzPresetsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PtzPresetsDialog)
{
    ui->setupUi(this);

    m_removeButton = new QPushButton(tr("Remove"));
    m_activateButton = new QPushButton(tr("Activate"));
    m_model = new QnPtzPresetListModel(this);

    ui->treeView->setModel(m_model);
    ui->treeView->setItemDelegateForColumn(m_model->column(QnPtzPresetListModel::HotkeyColumn), new QnPtzPresetHotkeyItemDelegate(this));
    ui->buttonBox->addButton(m_removeButton, QDialogButtonBox::HelpRole);
    ui->buttonBox->addButton(m_activateButton, QDialogButtonBox::HelpRole);

    connect(m_removeButton, SIGNAL(clicked()), this, SLOT(at_removeButton_clicked()));
    connect(m_activateButton, SIGNAL(clicked()), this, SLOT(at_activateButton_clicked()));
    connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(updateRemoveButtonEnabled()));
    connect(ui->treeView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(updateActivateButtonEnabled()));

    updateFromResource();
}

QnPtzPresetsDialog::~QnPtzPresetsDialog() {
    return;
}

const QnVirtualCameraResourcePtr &QnPtzPresetsDialog::camera() const {
    return m_camera;
}

void QnPtzPresetsDialog::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_camera == camera)
        return;

    if(m_camera)
        disconnect(m_camera, NULL, this, NULL);

    m_camera = camera;

    if(m_camera)
        connect(m_camera, SIGNAL(nameChanged(const QnResourcePtr &)), this, SLOT(updateLabel()));

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

void QnPtzPresetsDialog::submitToResource() {
    if(!m_camera)
        return;

    context()->instance<QnWorkbenchPtzPresetManager>()->setPtzPresets(m_camera, m_model->presets());
}

void QnPtzPresetsDialog::updateLabel() {
    ui->topLabel->setText(m_camera ? tr("PTZ presets for camera %1:").arg(m_camera->getName()) : QString());
}

void QnPtzPresetsDialog::updateModel() {
    m_model->setPresets(context()->instance<QnWorkbenchPtzPresetManager>()->ptzPresets(m_camera));
}

void QnPtzPresetsDialog::updateRemoveButtonEnabled() {
    m_removeButton->setEnabled(m_camera && ui->treeView->selectionModel()->selectedRows().size() > 0);
}

void QnPtzPresetsDialog::updateActivateButtonEnabled() {
    m_activateButton->setEnabled(m_camera && ui->treeView->currentIndex().isValid());
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnPtzPresetsDialog::at_removeButton_clicked() {
    /*QList<QnPtzPreset> indices
    foreach(const QModelIndex &index, ui->treeView->selectionModel()->selectedRows())
        selectedItems.insert(m_model->itemFromIndex(index));

    foreach(QStandardItem *item, selectedItems)
        m_model->invisibleRootItem()->removeRow(item->row());*/
}

void QnPtzPresetsDialog::at_activateButton_clicked() {
    QnPtzPreset preset = ui->treeView->currentIndex().data(Qn::PtzPresetRole).value<QnPtzPreset>();

    context()->menu()->trigger(Qn::PtzGoToPresetAction, QnActionParameters(m_camera).withArgument(Qn::ResourceNameRole, preset.name));
}



