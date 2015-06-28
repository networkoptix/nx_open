#include "ioport_settings_widget.h"
#include "ui_ioport_settings_widget.h"
#include "ui/models/ioports_actual_model.h"
#include "ui/delegates/ioport_item_delegate.h"

QnIOPortSettingsWidget::QnIOPortSettingsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::QnIOPortSettingsWidget)
{
    ui->setupUi(this);
    m_model = new QnIOPortsActualModel(this);
    ui->tableView->setModel(m_model);
    ui->tableView->setItemDelegate(new QnIOPortItemDelegate(this));  
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(false);
    //ui->tableView->resizeColumnsToContents();
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnIOPortsViewModel::NameColumn, QHeaderView::Stretch);
}

QnIOPortSettingsWidget::~QnIOPortSettingsWidget() {
}

void QnIOPortSettingsWidget::updateHeaderWidth()
{
    if (m_model->rowCount() == 0)
        return;
    
    QFontMetrics fm(ui->tableView->font());
    for (int j = 0; j <= QnIOPortsViewModel::DefaultStateColumn; ++j)
    {
        if (j == QnIOPortsViewModel::NameColumn)
            continue;
        int width = 0;
        for (int i = 0; i < m_model->rowCount(); ++i)
        {
            QModelIndex idx = m_model->index(i, j);
            QString text = m_model->data(idx).toString();
            width = qMax(width, fm.width(text));
        }
        ui->tableView->horizontalHeader()->resizeSection(j, width + 32);
    }
    QString text = m_model->headerData(QnIOPortsViewModel::AutoResetColumn, Qt::Horizontal).toString();
    ui->tableView->horizontalHeader()->resizeSection(QnIOPortsViewModel::AutoResetColumn, fm.width(text));
}

void QnIOPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr &camera)
{
    m_model->setModelData(camera ? camera->getIOPorts() : QnIOPortDataList());
    updateHeaderWidth();
}

void QnIOPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr &camera)
{
    if (camera)
        camera->setIOPorts(m_model->modelData());
}
