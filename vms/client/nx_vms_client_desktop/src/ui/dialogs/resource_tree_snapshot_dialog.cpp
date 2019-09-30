#include "resource_tree_snapshot_dialog.h"
#include "ui_resource_tree_snapshot_dialog.h"

#include <QtGui/QFontDatabase>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTextBrowser>

#include <ui/models/item_model_state_snapshot_helper.h>

using namespace nx::vms::client::desktop;

ResourceTreeSnapshotDialog::ResourceTreeSnapshotDialog(
    QAbstractItemModel* treeModel,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::ResourceTreeSnapshotDialog),
    m_treeModel(treeModel)
{
    ui->setupUi(this);
    ui->snapshotTextBrowser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui->snapshotTextBrowser->setWordWrapMode(QTextOption::NoWrap);

    for (auto lineEdit: findChildren<QLineEdit*>())
    {
        connect(lineEdit, &QLineEdit::textChanged,
            this, &ResourceTreeSnapshotDialog::updateShapshot);
    }
    for (auto checkBox: findChildren<QCheckBox*>())
    {
        connect(checkBox, &QCheckBox::stateChanged,
            this, &ResourceTreeSnapshotDialog::updateShapshot);
    }

    updateShapshot();
}

ResourceTreeSnapshotDialog::~ResourceTreeSnapshotDialog()
{
}

void nx::vms::client::desktop::ResourceTreeSnapshotDialog::updateShapshot()
{
    ItemModelStateSnapshotHelper::SnapshotParams params;

    params.parentIndex = indexFromString(ui->rootIndexLineEdit->text());

    bool ok = true;
    params.startRow = ui->startRowLineEdit->text().toInt(&ok);
    if (!ok)
        params.startRow.reset();
    params.rowCount = ui->rowCountLineEdit->text().toInt(&ok);
    if (!ok)
        params.rowCount.reset();
    params.depth = ui->depthLineEdit->text().toInt(&ok);
    if (!ok)
        params.depth.reset();

    params.roles.clear();
    if (ui->displayRoleCheckBox->checkState() == Qt::Checked)
        params.roles.insert(Qt::DisplayRole);
    if (ui->iconKeyRoleCheckbox->checkState() == Qt::Checked)
        params.roles.insert(Qn::ResourceIconKeyRole);
    if (ui->nodeTypeCheckBox->checkState() == Qt::Checked)
        params.roles.insert(Qn::NodeTypeRole);

    auto snapshotDocument = ItemModelStateSnapshotHelper::makeSnapshot(m_treeModel, params);

    ui->snapshotTextBrowser->setText(snapshotDocument.toJson());
}

QModelIndex ResourceTreeSnapshotDialog::indexFromString(const QString& string) const
{
    QModelIndex result;
    for (const auto& indexRowString: string.split(',', QString::SkipEmptyParts))
        result = m_treeModel->index(indexRowString.toInt(), 0, result);
    return result;
}
