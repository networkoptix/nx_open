#include "ioport_settings_widget.h"
#include "ui_ioport_settings_widget.h"

#include <core/resource/param.h>
#include <nx/fusion/model_functions.h>
#include <ui/delegates/ioport_item_delegate.h>
#include <ui/graphics/items/overlays/io_module_overlay_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ioports_view_model.h>


QnIOPortSettingsWidget::QnIOPortSettingsWidget(QWidget* parent):
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

    //TODO: #vkutin #gdm #common Change to usual hasChanges/hasChangesChanged logic
    connect(m_model, &QAbstractItemModel::dataChanged,
        this, &QnIOPortSettingsWidget::dataChanged);
    connect(ui->enableTileInterfaceCheckBox, &QCheckBox::clicked,
        this, &QnIOPortSettingsWidget::dataChanged);

    setHelpTopic(this, Qn::IOModules_Help);
}

QnIOPortSettingsWidget::~QnIOPortSettingsWidget()
{
}

void QnIOPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr& camera)
{
    QnIOPortDataList ports;
    auto style = QnIoModuleOverlayWidget::Style::Default;

    if (camera)
    {
        ports = camera->getIOPorts();
        style = QnLexical::deserialized<QnIoModuleOverlayWidget::Style>(
            camera->getProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME),
            QnIoModuleOverlayWidget::Style::Default);
    }

    m_model->setModelData(ports);
    ui->enableTileInterfaceCheckBox->setCheckState(style == QnIoModuleOverlayWidget::Style::Tile
        ? Qt::Checked
        : Qt::Unchecked);
}

void QnIOPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return;

    auto style = ui->enableTileInterfaceCheckBox->isChecked()
        ? QnIoModuleOverlayWidget::Style::Tile
        : QnIoModuleOverlayWidget::Style::Form;

    /* First, change style: */
    camera->setProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME, QnLexical::serialized(style));

    /* Second, change ports: */
    auto portList = m_model->modelData();
    if (!portList.empty()) //< can happen if it's just discovered unauthorized I/O module
        camera->setIOPorts(portList);
}
