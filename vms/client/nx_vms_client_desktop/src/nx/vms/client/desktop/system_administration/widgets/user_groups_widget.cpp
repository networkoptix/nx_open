// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_user_groups_widget.h"

#include <utility>

#include <QtCore/QCollator>
#include <QtCore/QPointer>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
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
#include <nx/vms/client/desktop/utils/ldap_status_watcher.h>
#include <nx/vms/client/desktop/workbench/managers/settings_dialog_manager.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>

#include "nx/vms/client/desktop/common/widgets/message_bar.h"
#include "private/highlighted_text_item_delegate.h"
#include "user_groups_widget.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

constexpr int kMaximumColumnWidth = 200;
constexpr int kMaximumInteractiveColumnWidth = 600;
constexpr int kDefaultInteractiveColumnWidth = 200;

static const core::SvgIconColorer::ThemeSubstitutions kTextButtonColors = {
    {QIcon::Normal, {"light4"}}, {QIcon::Active, {"light3"}}};

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
        const bool isGroupIconColumn = index.column() == UserGroupListModel::GroupTypeColumn
            || index.column() == UserGroupListModel::GroupWarningColumn;
        if (isGroupIconColumn || index.column() == UserGroupListModel::PermissionsColumn)
        {
            if (opt.state.testFlag(QStyle::StateFlag::State_Selected))
                painter->fillRect(opt.rect, opt.palette.highlight());

            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            if (icon.isNull())
                return;

            const auto horizontalAlignment = isGroupIconColumn ? Qt::AlignHCenter : Qt::AlignLeft;
            const qreal padding = isGroupIconColumn ? 0 : style::Metrics::kStandardPadding;

            const auto rect = QStyle::alignedRect(Qt::LeftToRight,
                horizontalAlignment | Qt::AlignVCenter,
                core::Skin::maximumSize(icon),
                opt.rect.adjusted(padding, 0, -padding, 0));

            icon.paint(painter, rect, Qt::AlignCenter,
                opt.state.testFlag(QStyle::StateFlag::State_Selected)
                    ? QIcon::Selected : QIcon::Normal);
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

        if (index.data(Qn::DisabledRole).toBool())
        {
            if (option->checkState == Qt::Unchecked)
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light13")
                        : core::colorTheme()->color("dark16"));
            }
            else
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light10")
                        : core::colorTheme()->color("light13"));
            }
        }
        else
        {
            if (option->checkState == Qt::Unchecked)
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light4")
                        : core::colorTheme()->color("light10"));
            }
            else
            {
                option->palette.setColor(QPalette::Text,
                    option->state.testFlag(QStyle::State_Selected)
                        ? core::colorTheme()->color("light2")
                        : core::colorTheme()->color("light4"));
            }
        }
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
    Q_DECLARE_TR_FUNCTIONS(UserGroupsWidget::Private)

    UserGroupsWidget* const q;

public:
    nx::utils::ImplPtr<Ui::UserGroupsWidget> ui{new Ui::UserGroupsWidget()};

    const QPointer<UserGroupManager> manager;
    UserGroupListModel* const groupsModel{new UserGroupListModel(q->systemContext(), q)};
    CustomizableSortFilterProxyModel* const sortModel{new CustomizableSortFilterProxyModel(q)};
    CheckableHeaderView* const header{new CheckableHeaderView(
        UserGroupListModel::CheckBoxColumn, q)};

    CommonMessageBar* const notFoundGroupsWarning{
        new CommonMessageBar(q, {.level = BarDescription::BarLevel::Error, .isClosable = true})};
    QPushButton* const deleteNotFoundGroupsButton{
        new QPushButton(qnSkin->icon("text_buttons/delete_20.svg", kTextButtonColors),
            QString(),
            notFoundGroupsWarning)};

    CommonMessageBar* const nonUniqueGroupsWarning{
        new CommonMessageBar(q, {.level = BarDescription::BarLevel::Info, .isClosable = true})};
    CommonMessageBar* const cycledGroupsWarning{
        new CommonMessageBar(q, {.level = BarDescription::BarLevel::Error, .isClosable = true})};

    ControlBar* const selectionControls{new ControlBar(q)};

    QPushButton* const deleteSelectedButton{new QPushButton(
        qnSkin->icon("text_buttons/delete_20.svg", kTextButtonColors),
        tr("Delete"),
        selectionControls)};

public:
    explicit Private(UserGroupsWidget* q, UserGroupManager* manager);
    void setupUi();

    void handleModelChanged();
    void handleSelectionChanged();
    void handleCellTriggered(const QModelIndex& index);

    nx::vms::api::UserGroupDataList userGroups() const;

    void updateBanners();
    void setMassDeleteInProgress(bool inProgress);

private:
    void createGroup();
    void deleteGroups(const QSet<QnUuid>& groupsToDelete);
    void editGroup(const QnUuid& groupId, bool raiseDialog = true);
    void deleteSelected();
    void deleteNotFoundLdapGroups();

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
    d->ui->groupsTable->setCurrentIndex({});
}

void UserGroupsWidget::resetWarnings()
{
    d->updateBanners();
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

    const auto updateCurrentGroupId =
        [this, q]()
        {
            const QnUuid groupId = q->context()->settingsDialogManager()->currentEditedGroupId();
            if (groupId.isNull())
            {
                ui->groupsTable->clearSelection();
                return;
            }

            const auto row = groupsModel->groupRow(groupId);
            const auto index = groupsModel->index(row);
            const auto mappedIndex = sortModel->mapFromSource(index);
            ui->groupsTable->setCurrentIndex(mappedIndex);
        };
    connect(q->context()->settingsDialogManager(),
        &SettingsDialogManager::currentEditedGroupIdChanged, this, updateCurrentGroupId);
    connect(q->context()->settingsDialogManager(),
        &SettingsDialogManager::currentEditedGroupIdChangeFailed, this, updateCurrentGroupId);
}

void UserGroupsWidget::Private::setupUi()
{
    ui->setupUi(q);
    q->addAction(ui->searchShortcut);
    ui->groupsTable->addAction(ui->deleteGroupAction);

    auto alertsLayout = new QVBoxLayout();
    alertsLayout->setSpacing(0);
    alertsLayout->addWidget(selectionControls);
    alertsLayout->addWidget(notFoundGroupsWarning);
    alertsLayout->addWidget(nonUniqueGroupsWarning);
    alertsLayout->addWidget(cycledGroupsWarning);
    q->layout()->addItem(alertsLayout);

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

    connect(ui->groupsTable, &QAbstractItemView::clicked, this, &Private::handleCellTriggered);
    connect(ui->groupsTable, &TreeView::spacePressed, this,
        [this](const QModelIndex& index)
        {
            handleCellTriggered(index.siblingAtColumn(UserGroupListModel::CheckBoxColumn));
        });
    ui->groupsTable->setEnterKeyEventIgnored(false);
    connect(ui->groupsTable, &TreeView::enterPressed, this,
        [this](const QModelIndex& index)
        {
            handleCellTriggered(index.siblingAtColumn(UserGroupListModel::NameColumn));
        });

    connect(ui->groupsTable, &TreeView::selectionChanging, this,
        [this](QItemSelectionModel::SelectionFlags selectionFlags,
            const QModelIndex& index,
            const QEvent* event)
        {
            if (!event || event->type() != QEvent::KeyPress)
                return;

            if (!selectionFlags.testFlag(QItemSelectionModel::Select))
                return;

            const auto groupId = index.data(Qn::UuidRole).value<QnUuid>();
            if (groupId.isNull())
                return;

            QMetaObject::invokeMethod(this,
                [this, groupId]() { editGroup(groupId, /*openDialog*/ false); },
                Qt::QueuedConnection);
        });

    connect(ui->groupsTable, &TreeView::gotFocus, this,
        [this](Qt::FocusReason reason)
        {
            if (reason != Qt::TabFocusReason && reason != Qt::BacktabFocusReason)
                return;

            if (ui->groupsTable->currentIndex().isValid())
                ui->groupsTable->setCurrentIndex(ui->groupsTable->currentIndex());
            else
                ui->groupsTable->setCurrentIndex(sortModel->index(0, 0));
        });
    connect(ui->groupsTable, &TreeView::lostFocus, this,
        [this](Qt::FocusReason reason)
        {
            if (!q->context()->settingsDialogManager()->isEditGroupDialogVisible())
                ui->groupsTable->clearSelection();
        });

    ui->createGroupButton->setIcon(qnSkin->icon("buttons/add_20x20.svg", kTextButtonColors));
    connect(ui->createGroupButton, &QPushButton::clicked, this, &Private::createGroup);

    nonUniqueGroupsWarning->setText(tr(
        "Multiple groups share the same name, which can lead to confusion. To maintain a clear "
            "and organized structure, we suggest providing unique names for each group."));
    cycledGroupsWarning->setText(tr(
        "Some groups have each other as both their parent and child members, or are part of such "
            "a circular reference chain. This can lead to incorrect calculations of "
            "permissions."));

    notFoundGroupsWarning->addButton(deleteNotFoundGroupsButton);
    connect(deleteNotFoundGroupsButton, &QPushButton::clicked, this,
        &Private::deleteNotFoundLdapGroups);

    deleteSelectedButton->setFlat(true);
    connect(deleteSelectedButton, &QPushButton::clicked, this, &Private::deleteSelected);

    constexpr int kButtonBarHeight = 40;
    selectionControls->setFixedHeight(kButtonBarHeight);

    setPaletteColor(selectionControls, QPalette::Window, core::colorTheme()->color("dark11"));
    setPaletteColor(selectionControls, QPalette::WindowText, core::colorTheme()->color("light4"));
    setPaletteColor(selectionControls, QPalette::Dark, core::colorTheme()->color("dark15"));
    QColor disabledTextColor = core::colorTheme()->color("light4");
    disabledTextColor.setAlphaF(0.3);
    setPaletteColor(selectionControls, QPalette::Disabled, QPalette::WindowText,
        disabledTextColor);
    setPaletteColor(deleteNotFoundGroupsButton, QPalette::Disabled, QPalette::WindowText,
        disabledTextColor);

    const auto buttonsLayout = selectionControls->horizontalLayout();
    buttonsLayout->setSpacing(16);
    buttonsLayout->addWidget(deleteSelectedButton);
    buttonsLayout->addStretch();

    QWidget::setTabOrder(ui->groupsTable, deleteSelectedButton);
    QWidget::setTabOrder(deleteSelectedButton, deleteNotFoundGroupsButton);

    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            sortModel->setFilterWildcard(text);
            q->update();
        });

    connect(ui->deleteGroupAction, &QAction::triggered, this,
        [this]()
        {
            const auto selected = visibleSelectedGroupIds();
            if (!selected.isEmpty())
            {
                deleteSelected();
                return;
            }

            const auto index = ui->groupsTable->currentIndex();
            if (!index.isValid())
                return;

            const auto groupId = index.data(Qn::UuidRole).value<QnUuid>();
            if (groupsModel->canDeleteGroup(groupId))
                deleteGroups({groupId});
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

    connect(q->systemContext()->ldapStatusWatcher(), &LdapStatusWatcher::statusChanged, this,
        &Private::updateBanners);
    connect(groupsModel, &UserGroupListModel::notFoundGroupsChanged, this,
        &Private::updateBanners);
    connect(groupsModel, &UserGroupListModel::nonUniqueGroupsChanged, this,
        &Private::updateBanners);
    connect(groupsModel, &UserGroupListModel::cycledGroupsChanged, this,
        &Private::updateBanners);

    handleSelectionChanged();
    updateBanners();
    setupPlaceholder();
}

void UserGroupsWidget::Private::setupPlaceholder()
{
    ui->nothingFoundPlaceholder->setFixedWidth(180);
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
    descriptionText->setWordWrap(true);
    descriptionText->setAlignment(Qt::AlignHCenter);
    font = descriptionText->font();
    font.setPixelSize(14);
    descriptionText->setFont(font);

    ui->nothingFoundPlaceholderLayout->addWidget(placeholderIcon, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addSpacing(15);
    ui->nothingFoundPlaceholderLayout->addWidget(placeholderText, /*stretch*/ 0, Qt::AlignHCenter);
    ui->nothingFoundPlaceholderLayout->addSpacing(9);
    ui->nothingFoundPlaceholderLayout->addWidget(descriptionText);
}

QSize UserGroupsWidget::sizeHint() const
{
    // We need to provide some valid size because of resizeContentsPrecision == 0. Without a valid
    // sizeHint parent layout may expand the viewport widget which leads to performance issues.
    return QSize(200, 200);
}

void UserGroupsWidget::Private::setMassDeleteInProgress(bool inProgress)
{
    ui->groupsTable->setDisabled(inProgress);
    ui->deleteGroupAction->setDisabled(inProgress);

    deleteSelectedButton->setDisabled(inProgress);
    deleteNotFoundGroupsButton->setDisabled(inProgress);
}

void UserGroupsWidget::Private::handleModelChanged()
{
    const bool empty = sortModel->rowCount() == 0;
    ui->searchWidget->setCurrentWidget(empty ? ui->nothingFoundPage : ui->groupsPage);
    handleSelectionChanged();
}

void UserGroupsWidget::Private::updateBanners()
{
    notFoundGroupsWarning->setText(
        tr("%n existing LDAP groups are not found in the LDAP database.", "",
            groupsModel->notFoundGroups().size()));
    deleteNotFoundGroupsButton->setText(
        tr("Delete %n groups", "", groupsModel->notFoundGroups().size()));

    notFoundGroupsWarning->setVisible(q->systemContext()->ldapStatusWatcher()->isOnline()
        && !groupsModel->notFoundGroups().isEmpty());
    nonUniqueGroupsWarning->setVisible(!groupsModel->nonUniqueGroups().isEmpty());
    cycledGroupsWarning->setVisible(!groupsModel->cycledGroups().isEmpty());
}

void UserGroupsWidget::Private::handleSelectionChanged()
{
    const auto visibleGroups = visibleGroupIds();
    const auto nonEditableGroupIds = groupsModel->nonEditableGroupIds() & visibleGroups;
    const auto checkedGroupIds = groupsModel->checkedGroupIds() & visibleGroups;

    if (checkedGroupIds.empty())
        header->setCheckState(Qt::Unchecked);
    else if ((checkedGroupIds.size() + nonEditableGroupIds.size()) == sortModel->rowCount())
        header->setCheckState(Qt::Checked);
    else
        header->setCheckState(Qt::PartiallyChecked);

    const bool canDelete = !checkedGroupIds.empty()
        && std::any_of(checkedGroupIds.cbegin(), checkedGroupIds.cend(),
            [this](const QnUuid& groupId) { return groupsModel->canDeleteGroup(groupId); });

    deleteSelectedButton->setVisible(canDelete);
    selectionControls->setDisplayed(canDelete);
}

void UserGroupsWidget::Private::handleCellTriggered(const QModelIndex& index)
{
    const auto groupId = index.data(Qn::UuidRole).value<QnUuid>();

    if (index.column() == UserGroupListModel::CheckBoxColumn)
    {
        const bool checked = (index.data(Qt::CheckStateRole).toInt() == Qt::Checked);
        groupsModel->setChecked(groupId, !checked);
        return;
    }

    editGroup(groupId);
}

void UserGroupsWidget::Private::createGroup()
{
    q->context()->settingsDialogManager()->createGroup(q);
}

void UserGroupsWidget::Private::editGroup(const QnUuid& groupId, bool raiseDialog)
{
    if (raiseDialog)
        q->context()->settingsDialogManager()->editGroup(groupId, q);
    else
        q->context()->settingsDialogManager()->setCurrentEditedGroupId(groupId);
}

void UserGroupsWidget::Private::deleteGroups(const QSet<QnUuid>& groupsToDelete)
{
    if (groupsToDelete.isEmpty())
        return;

    if (!ui::messages::UserGroups::removeGroups(q, groupsToDelete, /*allowSilent*/ false))
        return;

    setMassDeleteInProgress(true);
    auto rollback = nx::utils::makeScopeGuard([this]{ setMassDeleteInProgress(false); });

    GroupSettingsDialog::removeGroups(q->context(), groupsToDelete, nx::utils::guarded(q,
        [this, groupsToDelete, rollback = std::move(rollback)](
            bool success, const QString& errorString)
        {
            if (success)
                return;

            QString text;
            if (const auto count = groupsToDelete.count(); count == 1)
            {
                if (const auto group = groupsModel->findGroup(groupsToDelete.values()[0]))
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
}

void UserGroupsWidget::Private::deleteSelected()
{
    QSet<QnUuid> toDelete = visibleSelectedGroupIds();
    toDelete.removeIf([this](const QnUuid& id) { return !groupsModel->canDeleteGroup(id); });
    deleteGroups(toDelete);
}

void UserGroupsWidget::Private::deleteNotFoundLdapGroups()
{
    QSet<QnUuid> toDelete = groupsModel->notFoundGroups();
    toDelete.removeIf([this](const QnUuid& id) { return !groupsModel->canDeleteGroup(id); });
    deleteGroups(toDelete);
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
