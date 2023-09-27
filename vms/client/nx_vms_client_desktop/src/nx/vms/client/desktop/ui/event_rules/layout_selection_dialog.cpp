// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_selection_dialog.h"
#include "ui_layout_selection_dialog.h"

#include <QItemDelegate>
#include <QStylePainter>

#include <QtGui/QStandardItemModel>

#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/item_view_utils.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/style.h>
#include <ui/common/indents.h>
#include <utils/common/scoped_painter_rollback.h>

#include "private/subject_selection_dialog_p.h"

namespace nx::vms::client::desktop {

// It makes everything red.
CustomizableItemDelegate* makeRedDelegate(QObject* parent)
{
    auto delegate = new CustomizableItemDelegate(parent);
    delegate->setCustomInitStyleOption(
        [](QStyleOptionViewItem* item, const QModelIndex& /*index*/)
        {
            setWarningStyle(&item->palette);
        });
    return delegate;
}

LayoutSelectionDialog::LayoutSelectionDialog(
    bool singlePick,
    QWidget* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    ui(new Ui::LayoutSelectionDialog()),
    m_singlePick(singlePick)
{
    ui->setupUi(this);

    ui->alertBar->init({.level=BarDescription::BarLevel::Error});
    ui->additionalInfoLabel->setHidden(true);

    ui->localTreeView->setIgnoreWheelEvents(true);
    ui->sharedTreeView->setIgnoreWheelEvents(true);

    // Setup local layouts.
    m_localLayoutsModel = new QnResourceListModel(this);
    m_localLayoutsModel->setHasCheckboxes(true);
    m_localLayoutsModel->setSinglePick(m_singlePick);

    // Setup shared layouts.
    m_sharedLayoutsModel = new QnResourceListModel(this);
    m_sharedLayoutsModel->setHasCheckboxes(true);
    m_sharedLayoutsModel->setSinglePick(m_singlePick);

    // Making a filtered model for local layouts.
    auto filterLocalLayouts = std::make_shared<QSortFilterProxyModel>(m_localLayoutsModel);
    filterLocalLayouts->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterLocalLayouts->setFilterKeyColumn(QnResourceListModel::NameColumn);
    filterLocalLayouts->setSourceModel(m_localLayoutsModel);
    ui->localTreeView->setModel(filterLocalLayouts.get());

    // Making a filtered model for shared layouts.
    auto filterSharedLayouts = std::make_shared<QSortFilterProxyModel>(m_sharedLayoutsModel);
    filterSharedLayouts->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterSharedLayouts->setFilterKeyColumn(QnResourceListModel::NameColumn);
    filterSharedLayouts->setSourceModel(m_sharedLayoutsModel);
    ui->sharedTreeView->setModel(filterSharedLayouts.get());

    const auto setupTreeView =
        [singlePick](TreeView* treeView)
        {
            const QnIndents kIndents(1, 0);
            treeView->header()->setStretchLastSection(false);
            treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
            treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
            treeView->setProperty(style::Properties::kSideIndentation,
                QVariant::fromValue(kIndents));
            treeView->setDefaultSpacePressIgnored(true);
            item_view_utils::autoToggleOnRowClick(treeView, QnResourceListModel::CheckColumn);
            if (singlePick)
                treeView->setProperty(style::Properties::kItemViewRadioButtons, true);
        };

    setupTreeView(ui->localTreeView);
    setupTreeView(ui->sharedTreeView);

    auto scrollBar = new SnappedScrollBar(ui->mainWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    // Lambda will keep the reference to filterLocalLayouts and filterSharedLayouts.
    const auto updateFilter =
        [this, filterLocalLayouts, filterSharedLayouts]()
        {
            const auto filter = ui->searchLineEdit->text().trimmed();

            filterLocalLayouts->setFilterFixedString(filter);
            filterSharedLayouts->setFilterFixedString(filter);
        };

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this, updateFilter);
    connect(ui->searchLineEdit, &SearchLineEdit::enterKeyPressed, this, updateFilter);
    connect(ui->searchLineEdit, &SearchLineEdit::escKeyPressed, this,
        [this, updateFilter]()
        {
            ui->searchLineEdit->clear();
            updateFilter();
        });

    // We will hide some items if there are no data for them.
    const auto updateVisibility =
        [this]()
        {
            const bool noRoles = !ui->localTreeView->model()->rowCount();
            const bool noUsers = !ui->sharedTreeView->model()->rowCount();
            ui->localTreeView->setHidden(noRoles);
            ui->sharedTreeView->setHidden(noUsers);
            //ui->nothingFoundLabel->setVisible(noRoles && noUsers);
            update();
        };

    const auto connectToModelChanges =
        [this, updateVisibility](QAbstractItemModel* model)
        {
            connect(model, &QAbstractItemModel::modelReset,
                this, &LayoutSelectionDialog::at_layoutsChanged);
            connect(model, &QAbstractItemModel::rowsInserted,
                this, &LayoutSelectionDialog::at_layoutsChanged);
            connect(model, &QAbstractItemModel::rowsRemoved,
                this, &LayoutSelectionDialog::at_layoutsChanged);
            connect(model, &QAbstractItemModel::dataChanged,
                this, &LayoutSelectionDialog::at_layoutsChanged);

            connect(model, &QAbstractItemModel::rowsInserted, this, updateVisibility);
            connect(model, &QAbstractItemModel::rowsRemoved, this, updateVisibility);
            connect(model, &QAbstractItemModel::modelReset, this, updateVisibility);
        };

    connectToModelChanges(m_localLayoutsModel);
    connectToModelChanges(m_sharedLayoutsModel);

    if (singlePick)
    {
        connect(m_localLayoutsModel, &QnResourceListModel::selectionChanged, this,
            &LayoutSelectionDialog::at_localLayoutSelected);
        connect(m_sharedLayoutsModel, &QnResourceListModel::selectionChanged, this,
            &LayoutSelectionDialog::at_sharedLayoutSelected);
    }

    // Customized top margin for panel content.
    static constexpr int kContentTopMargin = 8;
    Style::setGroupBoxContentTopMargin(ui->localGroupBox, kContentTopMargin);
    Style::setGroupBoxContentTopMargin(ui->sharedGroupBox, kContentTopMargin);
}

LayoutSelectionDialog::~LayoutSelectionDialog()
{
}

void LayoutSelectionDialog::at_layoutsChanged()
{
    update();
}

void LayoutSelectionDialog::at_localLayoutSelected()
{
    auto selection = m_localLayoutsModel->checkedResources();
    if (!selection.empty())
        m_sharedLayoutsModel->setCheckedResources({});

    update();
}

void LayoutSelectionDialog::at_sharedLayoutSelected()
{
    auto selection = m_sharedLayoutsModel->checkedResources();
    if (!selection.empty())
        m_localLayoutsModel->setCheckedResources({});

    // Reset local layouts.
    if (m_localSelectionMode == ModeLimited)
    {
        m_localLayoutsModel->setResources(QnResourceList());
        ui->localGroupBox->setHidden(true);
    }
    update();
}

void LayoutSelectionDialog::setLocalLayouts(const QnResourceList& layouts,
    const QSet<QnUuid>& selection, LocalLayoutSelection mode)
{
    NX_ASSERT(m_localLayoutsModel);

    m_localSelectionMode = mode;

    if (m_localSelectionMode != ModeHideLocal)
    {
        m_localLayoutsModel->setResources(layouts);
        m_localLayoutsModel->setCheckedResources(selection);

        int selected = m_localLayoutsModel->checkedResources().size();

        if (m_localSelectionMode == ModeLimited)
        {
            ui->localGroupBox->setHidden(selected == 0);
            ui->localTreeView->setItemDelegate(makeRedDelegate(this));
        }
    }
    else
    {
        ui->localGroupBox->setHidden(true);
    }
}

void LayoutSelectionDialog::setSharedLayouts(const QnResourceList& layouts, const QSet<QnUuid>& selection)
{
    NX_ASSERT(m_sharedLayoutsModel);

    m_sharedLayoutsModel->setResources(layouts);
    m_sharedLayoutsModel->setCheckedResources(selection);
}

QSet<QnUuid> LayoutSelectionDialog::checkedLayouts() const
{
    QSet<QnUuid> result;

    auto local = m_localLayoutsModel->checkedResources();
    auto shared = m_sharedLayoutsModel->checkedResources();

    if (m_singlePick)
    {
        if (!local.empty())
            return QSet<QnUuid>{*local.begin()};
        if (!shared.empty())
            return QSet<QnUuid>{*shared.begin()};
    }
    result |= local;
    result |= shared;
    return result;
}

void LayoutSelectionDialog::showAlert(const QString& text)
{
    ui->alertBar->setHidden(text.isEmpty());
    ui->alertBar->setText(text);
}

void LayoutSelectionDialog::showInfo(const QString& text)
{
    ui->additionalInfoLabel->setHidden(text.isEmpty());
    ui->additionalInfoLabel->setText(text);
}

} // namespace nx::vms::client::desktop
