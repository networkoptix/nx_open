#include "legacy_ioport_settings_widget.h"
#include "ui_legacy_ioport_settings_widget.h"

#include <QtWidgets/QHeaderView>

#include "../widgets/private/io_ports_sort_model.h"

#include <core/resource/param.h>
#include <ui/delegates/ioport_item_delegate.h>
#include <ui/graphics/items/overlays/io_module_overlay_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ioports_view_model.h>
#include <nx/client/desktop/common/widgets/snapped_scroll_bar.h>

#include <nx/fusion/model_functions.h>

namespace nx::client::desktop {

LegacyIoPortSettingsWidget::LegacyIoPortSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::LegacyIoPortSettingsWidget),
    m_model(new QnIOPortsViewModel(this))
{
    auto sortModel = new IoPortsSortModel(this);
    sortModel->setSourceModel(m_model);

    ui->setupUi(this);
    ui->table->setModel(sortModel);
    ui->table->setItemDelegate(new QnIoPortItemDelegate(this));
    ui->table->setSortingEnabled(true);
    ui->table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->table->header()->setSectionResizeMode(QnIOPortsViewModel::NameColumn, QHeaderView::Stretch);
    ui->table->header()->setSortIndicator(QnIOPortsViewModel::NumberColumn, Qt::AscendingOrder);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(window());
    ui->table->setVerticalScrollBar(scrollBar->proxyScrollBar());

    // TODO: #vkutin #gdm #common Change to usual hasChanges/hasChangesChanged logic
    connect(m_model, &QAbstractItemModel::dataChanged,
        this, &LegacyIoPortSettingsWidget::dataChanged);
    connect(ui->enableTileInterfaceCheckBox, &QCheckBox::clicked,
        this, &LegacyIoPortSettingsWidget::dataChanged);

    setHelpTopic(this, Qn::IOModules_Help);
}

LegacyIoPortSettingsWidget::~LegacyIoPortSettingsWidget()
{
}

void LegacyIoPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr& camera)
{
    QnIOPortDataList ports;
    QnIoModuleOverlayWidget::Style style{};

    if (camera)
    {
        ports = camera->getIOPorts();
        style = QnLexical::deserialized<QnIoModuleOverlayWidget::Style>(
            camera->getProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME), {});
    }

    m_model->setModelData(ports);
    ui->enableTileInterfaceCheckBox->setCheckState(style == QnIoModuleOverlayWidget::Style::tile
        ? Qt::Checked
        : Qt::Unchecked);
}

void LegacyIoPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return;

    auto style = ui->enableTileInterfaceCheckBox->isChecked()
        ? QnIoModuleOverlayWidget::Style::tile
        : QnIoModuleOverlayWidget::Style::form;

    /* First, change style: */
    camera->setProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME, QnLexical::serialized(style));

    /* Second, change ports: */
    auto portList = m_model->modelData();
    if (!portList.empty()) //< can happen if it's just discovered unauthorized I/O module
        camera->setIOPorts(portList);
}

} // namespace nx::client::desktop
