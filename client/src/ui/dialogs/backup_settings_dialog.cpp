#include "backup_settings_dialog.h"
#include "core/resource/media_server_user_attributes.h"
#include "ui/widgets/views/checkboxed_header_view.h"

QnBackupSettingsDialog::QnBackupSettingsDialog(QWidget *parent, Qt::WindowFlags windowFlags ):
    base_type(parent, windowFlags),
    ui(new Ui::BackupSettingsDialog()),
    m_updatingModel(false),
    m_skipNextPressSignal(false)
{
    ui->setupUi(this);

    m_model = new QnBackupSettingsModel(this);
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(m_model);

    QnCheckBoxedHeaderView* headers = new QnCheckBoxedHeaderView(QnBackupSettingsModel::CheckBoxColumn, this);
    headers->setVisible(true);
    headers->setSectionsClickable(true);
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);
    headers->setStretchLastSection(true);
    ui->gridCameras->setHorizontalHeader(headers);
    ui->gridCameras->setModel(sortModel);

    connect (headers,           &QnCheckBoxedHeaderView::checkStateChanged, this, &QnBackupSettingsDialog::at_headerCheckStateChanged);

    connect(ui->checkBoxLQ, &QRadioButton::clicked, this, &QnBackupSettingsDialog::at_labelClicked);
    connect(ui->checkBoxHQ,  &QRadioButton::clicked, this, &QnBackupSettingsDialog::at_labelClicked);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &QnBackupSettingsDialog::at_modelDataChanged);
    connect(m_model, &QAbstractItemModel::modelReset, this, &QnBackupSettingsDialog::at_modelDataChanged);

    connect(ui->gridCameras->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QnBackupSettingsDialog::at_gridSelectionChanged);
    connect(ui->gridCameras,         &QTableView::pressed, this, &QnBackupSettingsDialog::at_gridItemPressed); // put selection changed before item pressed
}

void QnBackupSettingsDialog::at_gridSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QN_UNUSED(selected, deselected);

    QModelIndex mouseIdx = ui->gridCameras->mouseIndex();
    bool isCheckRow = mouseIdx.isValid() && mouseIdx.column() == 0;
    if (isCheckRow) {
        if (mouseIdx.data(Qt::CheckStateRole) == Qt::Unchecked)
            m_skipNextPressSignal = true;
    }
    else {
        m_model->blockSignals(true);
        m_model->setCheckState(Qt::Unchecked);
        m_model->blockSignals(false);
    }

    QModelIndexList indexes = ui->gridCameras->selectionModel()->selectedRows();
    if (m_skipNextSelIndex.isValid()) {
        for (auto itr = indexes.begin(); itr != indexes.end(); ++itr) {
            if (*itr == m_skipNextSelIndex) {
                indexes.erase(itr);
                m_skipNextPressSignal = false;
                break;
            }
        }
    }
    m_skipNextSelIndex = QModelIndex();
    
    m_updatingModel = true;
    for (const auto& index: indexes)
        ui->gridCameras->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
    m_updatingModel = false;
    at_modelDataChanged();
}

void QnBackupSettingsDialog::at_gridItemPressed(const QModelIndex& index)
{
    QnTableView* gridMaster = (QnTableView*) sender();

    if (m_skipNextPressSignal) {
        m_skipNextPressSignal = false;
        m_skipNextSelIndex = QModelIndex();
        return;
    }
    m_lastPressIndex = index;
    if (index.column() != QnBackupSettingsModel::CheckBoxColumn)
        return;

    m_skipNextSelIndex = index;

    Qt::CheckState checkState = (Qt::CheckState) index.data(Qt::CheckStateRole).toInt();
    if (checkState == Qt::Checked)
        checkState = Qt::Unchecked;
    else
        checkState = Qt::Checked;
    ui->gridCameras->model()->setData(index, checkState, Qt::CheckStateRole);
    //updateHeadersCheckState();
}

void QnBackupSettingsDialog::at_headerCheckStateChanged(Qt::CheckState state)
{
    if (state == Qt::PartiallyChecked)
        return;
    m_model->setCheckState(state);
}

void QnBackupSettingsDialog::at_modelDataChanged()
{
    if (m_updatingModel)
        return;

    ui->checkBoxLQ->blockSignals(true);
    ui->checkBoxHQ->blockSignals(true);
    auto headers = static_cast<QnCheckBoxedHeaderView*>(ui->gridCameras->horizontalHeader());
    headers->blockSignals(true);

    ui->checkBoxLQ->setChecked(Qt::Unchecked);
    ui->checkBoxHQ->setChecked(Qt::Unchecked);
    
    bool hasDisabled = false;
    bool hasHQ = false;
    bool hasLQ = false;

    for (const auto& value: m_model->modelData()) 
    {
        if (!value.isChecked)
            continue;

        switch(value.backupType) 
        {
        case Qn::CameraBackup_Disabled:
            hasDisabled = true;
            break;
        case Qn::CameraBackup_LowQuality:
            hasLQ = true;
            break;
        case Qn::CameraBackup_HighQuality:
            hasHQ = true;
            break;
        case Qn::CameraBackup_Both:
            hasLQ = true;
            hasHQ = true;
            break;
        default:
            break;
        }
    }

    if (hasDisabled && hasLQ)
        ui->checkBoxLQ->setCheckState(Qt::PartiallyChecked);
    else if (hasLQ)
        ui->checkBoxLQ->setCheckState(Qt::Checked);

    if (hasDisabled && hasHQ)
        ui->checkBoxHQ->setCheckState(Qt::PartiallyChecked);
    else if (hasHQ)
        ui->checkBoxHQ->setCheckState(Qt::Checked);

    headers->setCheckState(m_model->checkState());

    ui->checkBoxLQ->blockSignals(false);
    ui->checkBoxHQ->blockSignals(false);
    headers->blockSignals(false);

    QSortFilterProxyModel* proxyModel = (QSortFilterProxyModel*) ui->gridCameras->model();
    proxyModel->invalidate();
}

void QnBackupSettingsDialog::at_labelClicked()
{
    if (m_updatingModel)
        return;
    
    m_updatingModel = true;
    QCheckBox* checkBox = static_cast<QCheckBox*> (sender());
    if (checkBox->checkState() == Qt::PartiallyChecked)
        checkBox->setCheckState(Qt::Checked);

    QAbstractItemModel* model = ui->gridCameras->model();
    for (int i = 0; i < model->rowCount(); ++i)
    {
        QModelIndex index = model->index(i, 0);
        BackupSettingsData value = model->data(index, Qn::BackupSettingsDataRole).value<BackupSettingsData>();
        if (!value.isChecked)
            continue;

        if (ui->checkBoxLQ->checkState() == Qt::Checked)
            value.backupType |= Qn::CameraBackup_LowQuality;
        else if (ui->checkBoxLQ->checkState() == Qt::Unchecked)
            value.backupType &= ~Qn::CameraBackup_LowQuality;

        if (ui->checkBoxHQ->checkState() == Qt::Checked)
            value.backupType |= Qn::CameraBackup_HighQuality;
        else if (ui->checkBoxHQ->checkState() == Qt::Unchecked)
            value.backupType &= ~Qn::CameraBackup_HighQuality;
        
        model->setData(index, QVariant::fromValue<BackupSettingsData>(value), Qn::BackupSettingsDataRole);
    }
    m_updatingModel = false;

    
    QSortFilterProxyModel* proxyModel = (QSortFilterProxyModel*) ui->gridCameras->model();
    proxyModel->invalidate();
}

void QnBackupSettingsDialog::updateFromSettings(const BackupSettingsDataList& values)
{
    m_model->setModelData(values);
    ui->gridCameras->selectAll();
}

void QnBackupSettingsDialog::submitToSettings(BackupSettingsDataList& value)
{
    value = m_model->modelData();
}
