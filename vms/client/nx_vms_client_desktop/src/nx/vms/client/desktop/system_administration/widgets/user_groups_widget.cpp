// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_user_groups_widget.h"

#include <utility>

#include <QtCore/QCollator>
#include <QtCore/QPointer>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QStyledItemDelegate>

#include <client/client_globals.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/models/customizable_sort_filter_proxy_model.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/control_bars.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_administration/dialogs/group_settings_dialog.h>
#include <nx/vms/client/desktop/system_administration/models/user_group_list_model.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/messages/user_groups_messages.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>

#include "private/highlighted_text_item_delegate.h"
#include "user_groups_widget.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

constexpr int kMaximumColumnWidth = 200;
constexpr int kMaximumInteractiveColumnWidth = 600;
constexpr int kDefaultInteractiveColumnWidth = 200;

static const QMap<QIcon::Mode, nx::vms::client::core::SvgIconColorer::ThemeColorsRemapData>
    kTextButtonColors = {{QIcon::Normal, {"light14"}}, {QIcon::Active, {"light13"}}};

} // namespace

// -----------------------------------------------------------------------------------------------
// UserGroupsWidget::Delegate

class UserGroupsWidget::Delegate : public HighlightedTextItemDelegate
{
    using base_type = HighlightedTextItemDelegate;

public:
    explicit Delegate(QObject* parent = nullptr):
        base_type(parent, {
            UserGroupListModel::NameColumn,
            UserGroupListModel::ParentGroupsColumn})
    {
    }

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        switch (index.column())
        {
            case UserGroupListModel::GroupWarningColumn:
            case UserGroupListModel::GroupTypeColumn:
                return core::Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>());

            case UserGroupListModel::PermissionsColumn:
                return core::Skin::maximumSize(index.data(Qt::DecorationRole).value<QIcon>())
                    + QSize(style::Metrics::kStandardPadding * 2, 0);

            default:
            {
                const auto size = base_type::sizeHint(option, index);
                const auto maximumWidth = index.column() == UserGroupListModel::DescriptionColumn
                    ? kMaximumInteractiveColumnWidth
                    : kMaximumColumnWidth;

                return QSize(std::min(size.width(), maximumWidth), size.height());
            }
        }
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        auto opt = option;
        initStyleOption(&opt, index);

        // Paint center-aligned group type icon or left-aligned custom permissions icon:
        const bool isUserTypeColumn = index.column() == UserGroupListModel::GroupTypeColumn;
        if (isUserTypeColumn || index.column() == UserGroupListModel::PermissionsColumn)
        {
            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            const auto horizontalAlignment = isUserTypeColumn ? Qt::AlignHCenter : Qt::AlignLeft;
            const qreal padding = isUserTypeColumn ? 0 : style::Metrics::kStandardPadding;

            const auto rect = QStyle::alignedRect(Qt::LeftToRight,
                horizontalAlignment | Qt::AlignVCenter,
                core::Skin::maximumSize(icon),
                opt.rect.adjusted(padding, 0, -padding, 0));

            icon.paint(painter, rect, Qt::AlignCenter,
                opt.checkState == Qt::Checked ? QIcon::Selected : QIcon::Normal);
            return;
        }

        base_type::paint(painter, opt, index);
    }

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
    {
        base_type::initStyleOption(option, index);

        option->checkState = index.siblingAtColumn(UserGroupListModel::CheckBoxColumn).data(
            Qt::CheckStateRole).value<Qt::CheckState>();

        option->palette.setColor(QPalette::Text, option->checkState == Qt::Unchecked
            ? core::colorTheme()->color("light10")
            : core::colorTheme()->color("light4"));
    }

    virtual bool editorEvent(QEvent* event, QAbstractItemModel* model,
        const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        // Make clicks on a checkbox cell (not just checkbox indicator) toggle a check state.
        if (index.column() == UserGroupListModel::CheckBoxColumn
            && index.flags().testFlag(Qt::ItemIsUserCheckable)
            && event->type() == QEvent::MouseButtonRelease)
        {
            const auto mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                const auto currentState = index.data(Qt::CheckStateRole).value<Qt::CheckState>();
                const auto nextState = currentState == Qt::Checked ? Qt::Unchecked : Qt::Checked;
                model->setData(index, nextState, Qt::CheckStateRole);
                return true;
            }
        }

        return base_type::editorEvent(event, model, option, index);
    }
};

// -----------------------------------------------------------------------------------------------
// UserGroupsWidget::Private

class UserGroupsWidget::Private: public QObject
{
    UserGroupsWidget* const q;
    nx::utils::ImplPtr<Ui::UserGroupsWidget> ui{new Ui::UserGroupsWidget()};

public:
    const QPointer<UserGroupManager> manager;
    UserGroupListModel* const groupsModel{new UserGroupListModel(q->systemContext(), q)};
    CustomizableSortFilterProxyModel* const sortModel{new CustomizableSortFilterProxyModel(q)};
    CheckableHeaderView* const header{new CheckableHeaderView(
        UserGroupListModel::CheckBoxColumn, q)};

    ControlBar* const selectionControls{new ControlBar(q)};

    QPushButton* const deleteSelectedButton{new QPushButton(
        qnSkin->icon("text_buttons/delete_20_deprecated.svg", kTextButtonColors),
        tr("Delete"),
        selectionControls)};

public:
    explicit Private(UserGroupsWidget* q, UserGroupManager* manager);
    void setupUi();

    void handleModelChanged();
    void handleSelectionChanged();
    void handleCellClicked(const QModelIndex& index);

    nx::vms::api::UserGroupDataList userGroups() const;

    bool canDeleteGroup(const QnUuid& group);

private:
    void createGroup();
    void deleteSelected();

    void setupPlaceholder();

    QSet<QnUuid> visibleGroupIds() const;
    QSet<QnUuid> visibleSelectedGroupIds() const;
};

// -----------------------------------------------------------------------------------------------
// UserGroupsWidget

UserGroupsWidget::UserGroupsWidget(UserGroupManager* manager, QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this, manager))
{
    if (!NX_ASSERT(manager))
        return;

    d->setupUi();
    d->groupsModel->reset(d->userGroups());

    connect(manager, &UserGroupManager::addedOrUpdated, d.get(),
        [this](const nx::vms::api::UserGroupData& group)
        {
            if (group.attributes.testFlag(nx::vms::api::UserAttribute::hidden))
                d->groupsModel->removeGroup(group.id);
            else
                d->groupsModel->addOrUpdateGroup(group);
        });

    connect(manager, &UserGroupManager::removed, d.get(),
        [this](const nx::vms::api::UserGroupData& group)
        {
            d->groupsModel->removeGroup(group.id);
            emit hasChangesChanged();
        });

    connect(manager, &UserGroupManager::reset, d.get(),
        [this]()
        {
            loadDataToUi();
            emit hasChangesChanged();
        });
}

UserGroupsWidget::~UserGroupsWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void UserGroupsWidget::loadDataToUi()
{
    d->groupsModel->reset(d->userGroups());
}

void UserGroupsWidget::applyChanges()
{
    // TODO: Apply changes to visibleSelectedGroupIds().
}

bool UserGroupsWidget::hasChanges() const
{
    return false;
}

// -----------------------------------------------------------------------------------------------
// UserGroupsWidget::Private

UserGroupsWidget::Private::Private(UserGroupsWidget* q, UserGroupManager* manager):
    q(q),
    manager(manager)
{
    sortModel->setDynamicSortFilter(true);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setFilterKeyColumn(-1);
    sortModel->setFilterRole(Qn::FilterKeyRole);
    sortModel->setSortRole(Qn::SortKeyRole);
    sortModel->setSourceModel(groupsModel);

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    sortModel->setCustomLessThan(
        [collator, model = sortModel](const QModelIndex& left, const QModelIndex& right)
        {
            return collator(left.data(model->sortRole()).toString(),
                right.data(model->sortRole()).toString());
        });

    connect(sortModel, &QAbstractItemModel::rowsInserted, this, &Private::handleModelChanged);
    connect(sortModel, &QAbstractItemModel::rowsRemoved, this, &Private::handleModelChanged);
    connect(sortModel, &QAbstractItemModel::dataChanged, this, &Private::handleSelectionChanged);
    connect(sortModel, &QAbstractItemModel::modelReset, this, &Private::handleModelChanged);
}

void UserGroupsWidget::Private::setupUi()
{
    ui->setupUi(q);
    q->layout()->addWidget(selectionControls);

    const auto hoverTracker = new ItemViewHoverTracker(ui->groupsTable);

    ui->groupsTable->setModel(sortModel);
    ui->groupsTable->setHeader(header);

    auto delegate = new UserGroupsWidget::Delegate(q);
    ui->groupsTable->setItemDelegate(delegate);
    delegate->setView(ui->groupsTable);

    header->setVisible(true);
    header->setHighlightCheckedIndicator(true);
    header->setMaximumSectionSize(kMaximumInteractiveColumnWidth);
    header->setDefaultSectionSize(kDefaultInteractiveColumnWidth);
    header->setResizeContentsPrecision(0); //< Calculate resize using only the visible area.
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(UserGroupListModel::DescriptionColumn, QHeaderView::Interactive);
    header->setSectionResizeMode(UserGroupListModel::ParentGroupsColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    header->setSectionResizeMode(UserGroupListModel::GroupTypeColumn, QHeaderView::Fixed);
    header->resizeSection(UserGroupListModel::GroupTypeColumn, 28);
    header->setAlignment(UserGroupListModel::GroupTypeColumn, Qt::AlignHCenter);

    connect(header, &CheckableHeaderView::checkStateChanged, this,
        [this](Qt::CheckState checkState)
        {
            if (checkState == Qt::PartiallyChecked)
                return;

            groupsModel->setCheckedGroupIds(checkState == Qt::Checked
                ? visibleGroupIds()
                : QSet<QnUuid>());
        });

    ui->groupsTable->sortByColumn(UserGroupListModel::GroupTypeColumn, Qt::AscendingOrder);

    const auto scrollBar = new SnappedScrollBar(q->window());
    ui->groupsTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(ui->groupsTable, &QAbstractItemView::clicked, this, &Private::handleCellClicked);
    connect(ui->createGroupButton, &QPushButton::clicked, this, &Private::createGroup);

    deleteSelectedButton->setFlat(true);
    connect(deleteSelectedButton, &QPushButton::clicked, this, &Private::deleteSelected);

    constexpr int kButtonBarHeight = 32;
    selectionControls->setFixedHeight(kButtonBarHeight);

    setPaletteColor(selectionControls, QPalette::Window, core::colorTheme()->color("dark11"));
    setPaletteColor(selectionControls, QPalette::WindowText, core::colorTheme()->color("light14"));
    setPaletteColor(selectionControls, QPalette::Dark, core::colorTheme()->color("dark15"));

    const auto buttonsLayout = selectionControls->horizontalLayout();
    buttonsLayout->setSpacing(16);
    buttonsLayout->addWidget(deleteSelectedButton);
    buttonsLayout->addStretch();

    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            sortModel->setFilterWildcard(text);
            q->update();
        });

    setHelpTopic(q, HelpTopic::Id::SystemSettings_UserManagement);

    // Cursor changes with hover.
    connect(hoverTracker, &ItemViewHoverTracker::itemEnter, this,
        [this](const QModelIndex& index)
        {
            if (index.column() != UserGroupListModel::CheckBoxColumn)
                ui->groupsTable->setCursor(Qt::PointingHandCursor);
            else
                ui->groupsTable->unsetCursor();
        });

    connect(hoverTracker, &ItemViewHoverTracker::itemLeave, this,
        [this]() { ui->groupsTable->unsetCursor(); });

    handleSelectionChanged();

    setupPlaceholder();
}

void UserGroupsWidget::Private::setupPlaceholder()
{
    const auto placeholderIcon = new QLabel(ui->nothingFoundPlaceholder);
    placeholderIcon->setPixmap(qnSkin->pixmap("left_panel/placeholders/search.svg"));

    const auto placeholderText = new QLabel(ui->nothingFoundPlaceholder);
    placeholderText->setText(tr("No groups found"));
    QFont font(placeholderText->font());
    font.setPixelSize(16);
    font.setWeight(QFont::Medium);
    placeholderText->setFont(font);

    const auto descriptionText = new QLabel(ui->nothingFoundPlaceholder);
    descriptionText->setText(tr("Change search criteria or create a new group"));

    ui->nothingFoundPlaceholderLayout->addWidget(placeholderIcon, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addWidget(placeholderText, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addSpacing(8);
    ui->nothingFoundPlaceholderLayout->addWidget(descriptionText, /*stretch*/ 0, Qt::AlignHCenter);
}

QSize UserGroupsWidget::sizeHint() const
{
    // We need to provide some valid size because of resizeContentsPrecision == 0. Without a valid
    // sizeHint parent layout may expand the viewport widget which leads to performance issues.
    return QSize(200, 200);
}

void UserGroupsWidget::Private::handleModelChanged()
{
    const bool empty = sortModel->rowCount() == 0;
    ui->searchWidget->setCurrentWidget(empty ? ui->nothingFoundPage : ui->groupsPage);
    handleSelectionChanged();
}

bool UserGroupsWidget::Private::canDeleteGroup(const QnUuid& groupId)
{
    return q->systemContext()->accessController()->hasPermissions(
        groupId, Qn::RemovePermission);
}

void UserGroupsWidget::Private::handleSelectionChanged()
{
    const auto checkedGroupIds = groupsModel->checkedGroupIds() & visibleGroupIds();
    if (checkedGroupIds.empty())
        header->setCheckState(Qt::Unchecked);
    else if (checkedGroupIds.size() == sortModel->rowCount())
        header->setCheckState(Qt::Checked);
    else
        header->setCheckState(Qt::PartiallyChecked);

    const bool canDelete = !checkedGroupIds.empty()
        && std::any_of(checkedGroupIds.cbegin(), checkedGroupIds.cend(),
            [this](const QnUuid& groupId) { return canDeleteGroup(groupId); });

    deleteSelectedButton->setVisible(canDelete);
    selectionControls->setDisplayed(canDelete);
}

void UserGroupsWidget::Private::handleCellClicked(const QModelIndex& index)
{
    if (index.column() == UserGroupListModel::CheckBoxColumn)
        return;

    q->menu()->trigger(ui::action::UserGroupsAction, ui::action::Parameters()
        .withArgument(Qn::UuidRole, index.data(Qn::UuidRole).value<QnUuid>())
        .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
}

void UserGroupsWidget::Private::createGroup()
{
    q->menu()->triggerIfPossible(ui::action::UserGroupsAction, ui::action::Parameters()
        .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(q)));
}

void UserGroupsWidget::Private::deleteSelected()
{
    QSet<QnUuid> toDelete;

    const auto selectedGroups = visibleSelectedGroupIds();
    for (const auto& groupId: selectedGroups)
    {
        if (!canDeleteGroup(groupId))
            continue;

        toDelete << groupId;
    }

    if (toDelete.isEmpty())
        return;

    if (!ui::messages::UserGroups::removeGroups(q, toDelete, /*allowSilent*/ false))
        return;

    GroupSettingsDialog::removeGroups(q->systemContext(), toDelete, nx::utils::guarded(q,
        [this, toDelete](bool success, const QString& errorString)
        {
            q->setEnabled(true);

            if (success)
                return;

            QString text;
            if (const auto count = toDelete.count(); count == 1)
            {
                if (const auto group = groupsModel->findGroup(toDelete.values()[0]))
                    text = tr("Failed to delete group \"%1\".").arg(group->name.toHtmlEscaped());
                else
                    text = tr("Failed to delete group.");
            }
            else
            {
                text = tr("Failed to delete %n groups.", /*comment*/ "", count);
            }

            QnMessageBox messageBox(
                QnMessageBoxIcon::Critical,
                text,
                errorString,
                QDialogButtonBox::Ok,
                QDialogButtonBox::Ok,
                q);
            messageBox.exec();
        }));

    q->setEnabled(false);
}

nx::vms::api::UserGroupDataList UserGroupsWidget::Private::userGroups() const
{
    return manager->groups();
}

QSet<QnUuid> UserGroupsWidget::Private::visibleGroupIds() const
{
    QSet<QnUuid> result;
    for (int row = 0; row < sortModel->rowCount(); ++row)
    {
        const auto index = sortModel->index(row, 0);
        const auto groupId = index.data(Qn::UuidRole).value<QnUuid>();
        if (NX_ASSERT(!groupId.isNull()))
            result.insert(groupId);
    }

    return result;
}

QSet<QnUuid> UserGroupsWidget::Private::visibleSelectedGroupIds() const
{
    return visibleGroupIds() & groupsModel->checkedGroupIds();
}

} // namespace nx::vms::client::desktop
