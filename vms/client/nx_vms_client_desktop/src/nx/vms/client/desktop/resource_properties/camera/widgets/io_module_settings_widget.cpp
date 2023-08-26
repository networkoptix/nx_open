// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_module_settings_widget.h"
#include "ui_io_module_settings_widget.h"

#include <QtWidgets/QHeaderView>

#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/delegates/ioport_item_delegate.h>
#include <ui/models/ioports_view_model.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"
#include "private/io_ports_sort_model.h"

namespace nx::vms::client::desktop {

IoModuleSettingsWidget::IoModuleSettingsWidget(CameraSettingsDialogStore* store, QWidget* parent):
    base_type(parent),
    ui(new Ui::IoModuleSettingsWidget),
    m_model(new QnIOPortsViewModel(this))
{
    auto sortModel = new IoPortsSortModel(this);
    sortModel->setSourceModel(m_model);

    ui->setupUi(this);
    ui->table->setModel(sortModel);
    ui->table->setItemDelegate(new QnIoPortItemDelegate(this));
    ui->table->setSortingEnabled(true);

    auto header = ui->table->header();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnIOPortsViewModel::NameColumn, QHeaderView::Stretch);
    header->setSortIndicator(QnIOPortsViewModel::NumberColumn, Qt::AscendingOrder);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(window());
    ui->table->setVerticalScrollBar(scrollBar->proxyScrollBar());

    setContentsMargins(style::Metrics::kDefaultTopLevelMargins);
    setHelpTopic(this, HelpTopic::Id::IOModules);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &IoModuleSettingsWidget::loadState);

    connect(m_model, &QAbstractItemModel::dataChanged, this,
        [this, store]() { store->setIoPortDataList(m_model->modelData()); });

    connect(ui->enableTileInterfaceCheckBox, &QCheckBox::clicked, this,
        [this, store]()
        {
            store->setIoModuleVisualStyle(ui->enableTileInterfaceCheckBox->isChecked()
                ? vms::api::IoModuleVisualStyle::tile
                : vms::api::IoModuleVisualStyle::form);
        });
}

IoModuleSettingsWidget::~IoModuleSettingsWidget()
{
}

void IoModuleSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    m_model->setModelData(state.singleIoModuleSettings.ioPortsData());

    ui->enableTileInterfaceCheckBox->setChecked(
        state.singleIoModuleSettings.visualStyle() == vms::api::IoModuleVisualStyle::tile);
}

} // namespace nx::vms::client::desktop
