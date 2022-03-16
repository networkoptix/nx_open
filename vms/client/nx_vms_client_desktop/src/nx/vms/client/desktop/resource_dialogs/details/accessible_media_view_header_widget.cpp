// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_media_view_header_widget.h"

#include "ui_accessible_media_view_header_widget.h"

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <nx/vms/client/desktop/resource_dialogs/item_delegates/checkbox_column_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/resource_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/common/indents.h>

namespace {

static constexpr auto kHeaderViewExtraLeftIndent = 8;
static constexpr auto kHeaderViewSideIndents = QnIndents(
    nx::style::Metrics::kDefaultTopLevelMargin + kHeaderViewExtraLeftIndent,
    nx::style::Metrics::kDefaultTopLevelMargin);

} // namespace

namespace nx::vms::client::desktop {

AccessibleMediaViewHeaderWidget::AccessibleMediaViewHeaderWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::AccessibleMediaViewHeaderWidget()),
    m_allMediaCheckableItemModel(new QStandardItemModel())
{
    using namespace resource_selection_view;

    ui->setupUi(this);

    const auto allMediaTextItem = new QStandardItem(
        qnResIconCache->icon(QnResourceIconCache::Cameras),
        tr("All Cameras & Resources"));
    allMediaTextItem->setFlags({Qt::ItemIsEnabled});

    m_allMediaCheckableItem = new QStandardItem();
    m_allMediaCheckableItem->setCheckable(true);
    m_allMediaCheckableItem->setFlags({Qt::ItemIsEnabled});

    m_allMediaCheckableItemModel->appendRow({allMediaTextItem, m_allMediaCheckableItem});

    ui->allCamerasCheckboxView->setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(kHeaderViewSideIndents));
    ui->allCamerasCheckboxView->setFixedHeight(nx::style::Metrics::kViewRowHeight);
    ui->allCamerasCheckboxView->setModel(m_allMediaCheckableItemModel.get());
    ui->allCamerasCheckboxView->setItemDelegateForColumn(
        ResourceColumn, new ResourceDialogItemDelegate(this));
    ui->allCamerasCheckboxView->setItemDelegateForColumn(
        CheckboxColumn, new CheckBoxColumnItemDelegate(this));

    const auto checkboxColumnWidth =
        nx::style::Metrics::kCheckIndicatorSize + kHeaderViewSideIndents.right();

    const auto header = ui->allCamerasCheckboxView->header();
    header->setStretchLastSection(false);
    header->resizeSection(CheckboxColumn, checkboxColumnWidth);
    header->setSectionResizeMode(ResourceColumn, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(CheckboxColumn, QHeaderView::ResizeMode::Fixed);

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
