#include "ioport_settings_widget.h"
#include "ui_ioport_settings_widget.h"

#include <ui/delegates/ioport_item_delegate.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ioports_view_model.h>

QnIOPortSettingsWidget::QnIOPortSettingsWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    ui(new Ui::QnIOPortSettingsWidget),
    m_model(new QnIOPortsViewModel(this))
{
    ui->setupUi(this);
    ui->tableView->setModel(m_model);
    ui->tableView->setItemDelegate(new QnIOPortItemDelegate(this));
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnIOPortsViewModel::NameColumn, QHeaderView::Stretch);

    connect(m_model, &QAbstractItemModel::dataChanged, this, &QnIOPortSettingsWidget::dataChanged);

    setHelpTopic(this, Qn::IOModules_Help);
}

QnIOPortSettingsWidget::~QnIOPortSettingsWidget()
{ }

void QnIOPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr &camera)
{
    m_model->setModelData(camera ? camera->getIOPorts() : QnIOPortDataList());
}

void QnIOPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr &camera)
{
    if (camera)
        camera->setIOPorts(m_model->modelData());
}
