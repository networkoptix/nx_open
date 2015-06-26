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
    //ui->tableView->horizontalHeader()->setStretchLastSection(false);

    //ui->tableView->resizeColumnsToContents();
    //ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

QnIOPortSettingsWidget::~QnIOPortSettingsWidget() {
}

void QnIOPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr &camera)
{
    m_model->setModelData(camera ? camera->getIOPorts() : QnIOPortDataList());
}

void QnIOPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr &camera)
{
    if (camera)
        camera->setIOPorts(m_model->modelData());
}
