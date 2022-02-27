// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_media_view_header_widget.h"

#include "ui_accessible_media_view_header_widget.h"

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_selection_dialog_item_delegate.h>

namespace nx::vms::client::desktop {

AccessibleMediaViewHeaderWidget::AccessibleMediaViewHeaderWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::AccessibleMediaViewHeaderWidget()),
    m_allMediaCheckableItemModel(new QStandardItemModel())
{
    ui->setupUi(this);
    m_allMediaCheckableItem = new QStandardItem();
    m_allMediaCheckableItem->setIcon(qnResIconCache->icon(QnResourceIconCache::Cameras));
    m_allMediaCheckableItem->setText(tr("All Cameras & Resources"));
    m_allMediaCheckableItem->setCheckable(true);
    m_allMediaCheckableItem->setFlags({Qt::ItemIsEnabled});

    m_allMediaCheckableItemModel->appendRow(m_allMediaCheckableItem);

    ui->allCamerasCheckboxView->setFixedHeight(nx::style::Metrics::kViewRowHeight);
    ui->allCamerasCheckboxView->setModel(m_allMediaCheckableItemModel.get());
    ui->allCamerasCheckboxView->setItemDelegate(new ResourceSelectionDialogItemDelegate());

    connect(ui->allCamerasCheckboxView, &QTreeView::clicked, this,
        [this](const QModelIndex& index)
        {
            if (index.flags().testFlag(Qt::ItemIsEnabled))
                emit allMediaCheckableItemClicked();
        });
}

AccessibleMediaViewHeaderWidget::~AccessibleMediaViewHeaderWidget()
{
}

bool AccessibleMediaViewHeaderWidget::allMediaCheked()
{
    return m_allMediaCheckableItem->checkState() == Qt::Checked;
}

void AccessibleMediaViewHeaderWidget::setAllMediaCheked(bool checked)
{
    m_allMediaCheckableItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

} // namespace nx::vms::client::desktop
