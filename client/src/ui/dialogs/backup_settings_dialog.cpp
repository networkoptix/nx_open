#include "backup_settings_dialog.h"
#include "core/resource/media_server_user_attributes.h"
#include "ui/widgets/views/checkboxed_header_view.h"

namespace {
    static const int COLUMN_SPACING = 8;
}

class CustomSortFilterProxyModel : public QSortFilterProxyModel
{
    typedef QSortFilterProxyModel base_type;
public:
    CustomSortFilterProxyModel(QObject *parent = 0): QSortFilterProxyModel(parent) {}
protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (left.isValid() && right.isValid()) {
            BackupSettingsData l = left.data(Qn::BackupSettingsDataRole).value<BackupSettingsData>();
            BackupSettingsData r = right.data(Qn::BackupSettingsDataRole).value<BackupSettingsData>();
            bool leftIsNull = l.id.isNull();
            bool rightIsNull = r.id.isNull();
            if (leftIsNull != rightIsNull)
                return leftIsNull < rightIsNull;
        }
        return base_type::lessThan(left, right);
    }
};

class QnCustomItemDelegate: public QStyledItemDelegate 
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnCustomItemDelegate(QObject *parent = NULL): base_type(parent) {}

    virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override
    {
        QSize result = base_type::sizeHint(option, index);
        result.setWidth(result.width() + COLUMN_SPACING);
        return result;
    }
};


QnBackupSettingsDialog::QnBackupSettingsDialog(QWidget *parent, Qt::WindowFlags windowFlags ):
    base_type(parent, windowFlags),
    ui(new Ui::BackupSettingsDialog()),
    m_updatingModel(false)
{
    ui->setupUi(this);

    m_model = new QnBackupSettingsModel(this);
    auto sortModel = new CustomSortFilterProxyModel(this);
    sortModel->setSourceModel(m_model);

    QnCheckBoxedHeaderView* headers = new QnCheckBoxedHeaderView(QnBackupSettingsModel::CheckBoxColumn, this);
    headers->setVisible(true);
    headers->setSectionsClickable(true);
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);
    headers->setStretchLastSection(true);
    ui->gridCameras->setHorizontalHeader(headers);
    ui->gridCameras->setModel(sortModel);

    ui->gridCameras->setItemDelegate(new QnCustomItemDelegate(this));

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
    }
    else {
        m_model->blockSignals(true);
        m_model->setCheckState(Qt::Unchecked);
        m_model->blockSignals(false);
    }

    QModelIndexList indexes = ui->gridCameras->selectionModel()->selectedRows();
    
    m_updatingModel = true;
    for (const auto& index: indexes)
        ui->gridCameras->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
    m_updatingModel = false;
    at_modelDataChanged();
}

void QnBackupSettingsDialog::at_gridItemPressed(const QModelIndex& index)
{
}

void QnBackupSettingsDialog::at_headerCheckStateChanged(Qt::CheckState state)
{
    if (state == Qt::PartiallyChecked)
        return;
    if (state == Qt::Checked)
        ui->gridCameras->selectAll();
    else
        ui->gridCameras->selectionModel()->clearSelection();
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
    
    bool hasHQ = false;
    bool hasLQ = false;
    bool notHasHQ = false;
    bool notHasLQ = false;

    for (const auto& value: m_model->modelData()) 
    {
        if (!value.isChecked)
            continue;

        if (value.backupType & Qn::CameraBackup_LowQuality)
            hasLQ = true;
        else
            notHasLQ = true;

        if (value.backupType & Qn::CameraBackup_HighQuality)
            hasHQ = true;
        else
            notHasHQ = true;
    }

    if (hasLQ && notHasLQ)
        ui->checkBoxLQ->setCheckState(Qt::PartiallyChecked);
    else if (hasLQ)
        ui->checkBoxLQ->setCheckState(Qt::Checked);

    if (hasHQ && notHasHQ)
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

void QnBackupSettingsDialog::updateFromSettings(const BackupSettingsDataList& values, const QnMediaServerUserAttributes& serverAttrs)
{
    auto modelData = values;
    for (auto& value: modelData) {
        if (value.backupType == Qn::CameraBackup_Default)
            value.backupType = serverAttrs.backupQuality;
    }

    BackupSettingsData allCameras;
    allCameras.backupType = serverAttrs.backupQuality;
    modelData << allCameras;

    m_model->setModelData(modelData);
    ui->gridCameras->selectAll();
}

void QnBackupSettingsDialog::submitToSettings(BackupSettingsDataList& value, QnMediaServerUserAttributes& serverAttrs)
{
    BackupSettingsDataList modelData = m_model->modelData();
    for (auto itr = modelData.begin(); itr != modelData.end(); ++itr)
    {
        if (itr->id.isNull()) 
        {
            serverAttrs.backupQuality = itr->backupType;
            modelData.erase(itr); // remove footage data
            break;
        }
    }

    value = modelData;
}
