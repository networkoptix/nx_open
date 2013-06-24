#include "ptz_presets_dialog.h"
#include "ui_ptz_presets_dialog.h"

#include <QtGui/QPushButton>
#include <QStandardItem>

#include <core/resource/camera_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/actions/action_manager.h>


namespace {
    enum DataRole {
        LogicalPositionRole = Qt::UserRole,
        HotkeyRole
    };

    enum Column {
        NameColumn,
        HotkeyColumn
    };
}

QnPtzPresetsDialog::QnPtzPresetsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PtzPresetsDialog)
{
    ui->setupUi(this);

    m_removeButton = new QPushButton(tr("Remove"));
    m_activateButton = new QPushButton(tr("Activate"));
    m_model = new QStandardItemModel(this);

    ui->treeView->setModel(m_model);
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

    QList<QnPtzPreset> presets;
    
    QStandardItem *root = m_model->invisibleRootItem();
    for(int row = 0, rowCount = root->rowCount(); row < rowCount; row++) {
        QStandardItem *item = root->child(row);

        presets.push_back(QnPtzPreset(item->data(HotkeyRole).toInt(), item->text(), item->data(LogicalPositionRole).value<QVector3D>()));
    }

    context()->instance<QnWorkbenchPtzPresetManager>()->setPtzPresets(m_camera, presets);
}

void QnPtzPresetsDialog::updateLabel() {
    ui->topLabel->setText(m_camera ? tr("PTZ presets for camera %1:").arg(m_camera->getName()) : QString());
}

void QnPtzPresetsDialog::updateModel() {
    m_model->clear();
    if(!m_camera)
        return;

    m_model->setColumnCount(2);
    m_model->setHorizontalHeaderItem(NameColumn, new QStandardItem(tr("Name")));
    m_model->setHorizontalHeaderItem(HotkeyColumn, new QStandardItem(tr("Hotkey")));

    QList<QnPtzPreset> presets = context()->instance<QnWorkbenchPtzPresetManager>()->ptzPresets(m_camera);
    foreach(const QnPtzPreset &preset, presets) {
        QStandardItem *nameItem = new QStandardItem(preset.name);
        nameItem->setData(QVariant::fromValue<QVector3D>(preset.logicalPosition), LogicalPositionRole);
        nameItem->setData(QVariant::fromValue(preset.hotkey), HotkeyRole);
        
        QStandardItem *hotkeyItem = new QStandardItem(preset.hotkey < 0 ? tr("None") : QString::number(preset.hotkey));
        
        int row = m_model->rowCount();
        m_model->setItem(row, NameColumn, nameItem);
        m_model->setItem(row, HotkeyColumn, hotkeyItem);
    }
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
    QSet<QStandardItem *> selectedItems;
    foreach(const QModelIndex &index, ui->treeView->selectionModel()->selectedRows())
        selectedItems.insert(m_model->itemFromIndex(index));

    foreach(QStandardItem *item, selectedItems)
        m_model->invisibleRootItem()->removeRow(item->row());
}

void QnPtzPresetsDialog::at_activateButton_clicked() {
    QStandardItem *item = m_model->itemFromIndex(ui->treeView->currentIndex());
    if(!item)
        return;

    context()->menu()->trigger(Qn::PtzGoToPresetAction, QnActionParameters(m_camera).withArgument(Qn::ResourceNameRole, item->text()));
}



