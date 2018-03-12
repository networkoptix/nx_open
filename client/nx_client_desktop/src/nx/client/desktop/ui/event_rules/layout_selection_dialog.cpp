#include "layout_selection_dialog.h"
#include "private/subject_selection_dialog_p.h"
#include <ui_layout_selection_dialog.h> //< generated file

#include <QtGui/QStandardItemModel>
#include <QItemDelegate>
#include <QStylePainter>

#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <ui/common/indents.h>
#include <ui/style/helper.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/string.h>


#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/common/item_view_utils.h>
#include <nx/client/desktop/common/models/natural_string_sort_proxy_model.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

QnCustomizableItemDelegate* makeRadioButtonDelegate(QObject* parent)
{
    auto delegate = new QnCustomizableItemDelegate(parent);

    delegate->setCustomPaint(
        [](QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            const QWidget* widget = option.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();

            QStyleOptionButton button;
            button.rect = option.rect;
            button.state = QStyle::State_Enabled;
            button.palette = option.palette;

            bool parseOk = false;
            int state = index.data(Qt::CheckStateRole).toInt(&parseOk);
            if (option.checkState == Qt::Checked || state == Qt::Checked)
                button.state |= QStyle::State_On;

            style->drawControl(QStyle::CE_RadioButton, &button, painter);
        });

    delegate->setCustomSizeHint(
        [](const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            const QWidget* widget = option.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();
            QSize result = style->sizeFromContents(QStyle::CT_CheckBox, &option, QSize(5, 5), widget);
            return result;
        });
    return delegate;
}

// It makes everything red
QnCustomizableItemDelegate* makeRedDelegate(QObject* parent)
{
    auto delegate = new QnCustomizableItemDelegate(parent);
    delegate->setCustomInitStyleOption(
        [](QStyleOptionViewItem* item, const QModelIndex& index)
        {
            QColor color = qnGlobals->errorTextColor();
            item->palette.setColor(QPalette::Text, color);
        });
    return delegate;
}

LayoutSelectionDialog::LayoutSelectionDialog(bool singlePick, QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LayoutSelectionDialog()),
    m_singlePick(singlePick)
{
    ui->setupUi(this);
    ui->additionalInfoLabel->setHidden(true);

    // Setup local layouts
    m_localLayoutsModel = new QnResourceListModel(this);
    m_localLayoutsModel->setHasCheckboxes(true);
    m_localLayoutsModel->setSinglePick(m_singlePick);
    

    // Setup shared layouts
    m_sharedLayoutsModel = new QnResourceListModel(this);
    m_sharedLayoutsModel->setSinglePick(m_singlePick);
    m_sharedLayoutsModel->setHasCheckboxes(true);

    auto radioButtonDelegate = makeRadioButtonDelegate(this);

    // Making a filtered model for local layouts.
    auto filterLocalLayouts = std::make_shared<QSortFilterProxyModel>(m_localLayoutsModel);
    filterLocalLayouts->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterLocalLayouts->setFilterKeyColumn(QnResourceListModel::NameColumn);
    filterLocalLayouts->setSourceModel(m_localLayoutsModel);
    ui->localTreeView->setModel(filterLocalLayouts.get());
    ui->localTreeView->setItemDelegateForColumn(QnResourceListModel::CheckColumn, radioButtonDelegate);

    // Making a filtered model for shared layouts.
    auto filterSharedLayouts = std::make_shared<QSortFilterProxyModel>(m_sharedLayoutsModel);
    filterSharedLayouts->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterSharedLayouts->setFilterKeyColumn(QnResourceListModel::NameColumn);
    filterSharedLayouts->setSourceModel(m_sharedLayoutsModel);
    ui->sharedTreeView->setModel(filterSharedLayouts.get());

    // Lambda will keep the reference to filterLocalLayouts and filterSharedLayouts

    auto indicatorDelegate = new QnCustomizableItemDelegate(this);
    indicatorDelegate->setCustomSizeHint(
        [](const QStyleOptionViewItem& option, const QModelIndex& /*index*/)
        {
            return qnSkin->maximumSize(option.icon);
        });

    indicatorDelegate->setCustomPaint(
        [](QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& /*index*/)
        {
            option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem,
                &option, painter, option.widget);
            QnScopedPainterOpacityRollback opacityRollback(painter);
            const bool selected = option.state.testFlag(QStyle::State_Selected);
            if (selected)
                painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);
            option.icon.paint(painter, option.rect, Qt::AlignCenter,
                selected ? QIcon::Normal : QIcon::Disabled);
        });
    
    auto setupTreeView =
        [this, radioButtonDelegate](QnTreeView* treeView)
        {
            const QnIndents kIndents(1, 0);
            treeView->header()->setStretchLastSection(false);
            treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
            treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
            treeView->setProperty(style::Properties::kSideIndentation,
                QVariant::fromValue(kIndents));
            treeView->setIgnoreDefaultSpace(true);
            ItemViewUtils::autoToggleOnRowClick(treeView, QnResourceListModel::CheckColumn);
            treeView->setItemDelegateForColumn(QnResourceListModel::CheckColumn, radioButtonDelegate);
        };

    setupTreeView(ui->localTreeView);
    setupTreeView(ui->sharedTreeView);

    auto scrollBar = new QnSnappedScrollBar(ui->mainWidget);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    const auto updateFilter =
        [this, filterLocalLayouts, filterSharedLayouts]()
        {
            const auto filter = ui->searchLineEdit->text().trimmed();

            filterLocalLayouts->setFilterFixedString(filter);
            filterSharedLayouts->setFilterFixedString(filter);
        };

    connect(ui->searchLineEdit, &QnSearchLineEdit::textChanged, this, updateFilter);
    connect(ui->searchLineEdit, &QnSearchLineEdit::enterKeyPressed, this, updateFilter);
    connect(ui->searchLineEdit, &QnSearchLineEdit::escKeyPressed, this,
        [this, updateFilter]()
        {
            ui->searchLineEdit->clear();
            updateFilter();
        });

    
    const auto connectToModelChanges =
        [this](QAbstractItemModel* model)
        {
            connect(model, &QAbstractItemModel::modelReset,
                this, &LayoutSelectionDialog::at_layoutsChanged);
            connect(model, &QAbstractItemModel::rowsInserted,
                this, &LayoutSelectionDialog::at_layoutsChanged);
            connect(model, &QAbstractItemModel::rowsRemoved,
                this, &LayoutSelectionDialog::at_layoutsChanged);
            connect(model, &QAbstractItemModel::dataChanged,
                this, &LayoutSelectionDialog::at_layoutsChanged);
        };

    connectToModelChanges(m_localLayoutsModel);
    connectToModelChanges(m_sharedLayoutsModel);

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

    connect(m_localLayoutsModel, &QAbstractItemModel::rowsInserted, this, updateVisibility);
    connect(m_localLayoutsModel, &QAbstractItemModel::rowsRemoved, this, updateVisibility);
    connect(m_localLayoutsModel, &QAbstractItemModel::modelReset, this, updateVisibility);
    connect(m_localLayoutsModel, &QnResourceListModel::selectionChanged, this, &LayoutSelectionDialog::at_localLayoutSelected);
    connect(m_sharedLayoutsModel, &QAbstractItemModel::rowsInserted, this, updateVisibility);
    connect(m_sharedLayoutsModel, &QAbstractItemModel::rowsRemoved, this, updateVisibility);
    connect(m_sharedLayoutsModel, &QAbstractItemModel::modelReset, this, updateVisibility);
    connect(m_sharedLayoutsModel, &QnResourceListModel::selectionChanged, this, &LayoutSelectionDialog::at_sharedLayoutSelected);
    
    // Customized top margin for panel content:
    static constexpr int kContentTopMargin = 8;
    QnNxStyle::setGroupBoxContentTopMargin(ui->localGroupBox, kContentTopMargin);
    QnNxStyle::setGroupBoxContentTopMargin(ui->sharedGroupBox, kContentTopMargin);
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
    {
        m_sharedLayoutsModel->setCheckedResources(QSet<QnUuid>());
    }
    update();
}

void LayoutSelectionDialog::at_sharedLayoutSelected()
{
    auto selection = m_sharedLayoutsModel->checkedResources();
    if (!selection.empty())
    {
        m_localLayoutsModel->setCheckedResources(QSet<QnUuid>());
    }

    // Reset local layouts
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
        else if (!shared.empty())
            return QSet<QnUuid>{*shared.begin()};
    }
    result &= local;
    result &= shared;
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

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
