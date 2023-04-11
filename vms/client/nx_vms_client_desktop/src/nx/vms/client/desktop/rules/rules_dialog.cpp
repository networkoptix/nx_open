// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_dialog.h"
#include "ui_rules_dialog.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QCloseEvent>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/rules/model_view/modification_mark_item_delegate.h>
#include <nx/vms/client/desktop/rules/model_view/rules_sort_filter_proxy_model.h>
#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/client/desktop/rules/params_widgets/editor_factory.h>
#include <nx/vms/client/desktop/rules/params_widgets/params_widget.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/events/generic_event.h>
#include <ui/common/indents.h>
#include <ui/common/palette.h>
#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop::rules {

RulesDialog::RulesDialog(QWidget* parent):
    QnSessionAwareButtonBoxDialog(parent),
    m_ui(new Ui::RulesDialog()),
    m_rulesTableModel(
        new RulesTableModel(appContext()->currentSystemContext()->vmsRulesEngine(), this)),
    m_rulesFilterModel(new RulesSortFilterProxyModel(this))
{
    m_ui->setupUi(this);

    const auto buttonIconColor = QPalette().color(QPalette::BrightText);

    m_ui->newRuleButton->setIcon(core::Skin::colorize(
        qnSkin->pixmap("text_buttons/arythmetic_plus.png"), buttonIconColor));
    m_ui->deleteRuleButton->setIcon(core::Skin::colorize(
        qnSkin->pixmap("text_buttons/trash.png"), buttonIconColor));
    m_ui->deleteRuleButton->setEnabled(false);

    m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    const auto midlightColor = QPalette().color(QPalette::Midlight);
    setPaletteColor(m_ui->searchLineEdit, QPalette::PlaceholderText, midlightColor);

    m_ui->actionTypePicker->init(appContext()->currentSystemContext()->vmsRulesEngine());
    m_ui->eventTypePicker->init(appContext()->currentSystemContext()->vmsRulesEngine());

    connect(m_rulesTableModel, &RulesTableModel::dataChanged, this, &RulesDialog::onModelDataChanged);
    connect(m_rulesTableModel, &RulesTableModel::stateChanged, this, &RulesDialog::updateControlButtons);

    connect(m_ui->newRuleButton, &QPushButton::clicked, this,
        [this]
        {
            resetFilter();

            auto newRuleIndex = m_rulesTableModel->addRule(
                vms::rules::GenericEvent::manifest().id,
                vms::rules::NotificationAction::manifest().id);
            m_ui->tableView->selectionModel()->setCurrentIndex(m_rulesFilterModel->mapFromSource(newRuleIndex),
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        });

    connect(m_ui->deleteRuleButton, &QPushButton::clicked, this, &RulesDialog::deleteCurrentRule);

    connect(m_ui->eventTypePicker, &EventTypePickerWidget::eventTypePicked, this,
        [this](const QString& eventType)
        {
            if (auto rule = m_displayedRule.lock())
                rule->setEventType(eventType);
        });

    connect(m_ui->actionTypePicker, &ActionTypePickerWidget::actionTypePicked, this,
        [this](const QString& actionType)
        {
            if (auto rule = m_displayedRule.lock())
                rule->setActionType(actionType);
        });

    connect(m_ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
        [this]
        {
            applyChanges();
        });

    connect(m_ui->resetDefaultRulesButton, &QPushButton::clicked, this, &RulesDialog::resetToDefaults);

    setupRuleTableView();

    m_readOnly = false; //< TODO: Get real readonly state.
    updateReadOnlyState();
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
RulesDialog::~RulesDialog() = default;

bool RulesDialog::tryClose(bool force)
{
    if (force)
    {
        rejectChanges();
        if (!isHidden())
            hide();

        return true;
    }

    if (!m_rulesTableModel->hasChanges())
        return true;

    const auto result = QnMessageBox::question(
        this,
        tr("Apply changes before exiting?"),
        QString(),
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply);

    switch (result)
    {
        case QDialogButtonBox::Apply:
            applyChanges();
            break;
        case QDialogButtonBox::Discard:
            rejectChanges();
            break;
        default:
            return false; // Cancel was pressed
    }

    return true;
}

void RulesDialog::showEvent(QShowEvent* event)
{
    QnSessionAwareButtonBoxDialog::showEvent(event);

    resetFilter();
    resetSelection();
    m_ui->searchLineEdit->setFocus();
}

void RulesDialog::closeEvent(QCloseEvent* event)
{
    QnSessionAwareButtonBoxDialog::closeEvent(event);

    if (event->isAccepted())
        m_displayedRule.reset();
}

bool RulesDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        auto pKeyEvent = static_cast<QKeyEvent*>(event);
        switch (pKeyEvent->key())
        {
            case Qt::Key_Delete:
#if defined(Q_OS_MAC)
            case Qt::Key_Backspace:
#endif
                deleteCurrentRule();
                return true;
            default:
                break;
        }
    }

    return QnSessionAwareButtonBoxDialog::eventFilter(object, event);
}

void RulesDialog::accept()
{
    applyChanges();

    QnSessionAwareButtonBoxDialog::accept();
}

void RulesDialog::reject()
{
    if (!tryClose(false))
        return;

    QnSessionAwareButtonBoxDialog::reject();
}

void RulesDialog::setupRuleTableView()
{
    m_ui->tableView->installEventFilter(this);
    m_ui->tableView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(
        QnIndents(style::Metrics::kStandardPadding, style::Metrics::kStandardPadding)));

    auto scrollBar = new SnappedScrollBar(m_ui->tableHolder);
    m_ui->tableView->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseItemViewPaddingWhenVisible(true);

    m_rulesFilterModel->setSourceModel(m_rulesTableModel);
    m_rulesFilterModel->setFilterRole(RulesTableModel::FilterRole);
    m_rulesFilterModel->setDynamicSortFilter(true);
    m_rulesFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_rulesFilterModel->sort(RulesTableModel::EventColumn);

    m_ui->tableView->setModel(m_rulesFilterModel);
    m_ui->tableView->setItemDelegateForColumn(RulesTableModel::EditedStateColumn,
        new ModificationMarkItemDelegate(m_ui->tableView));
    m_ui->tableView->setItemDelegateForColumn(RulesTableModel::EnabledStateColumn,
        new SwitchItemDelegate(m_ui->tableView));
    m_ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto horizontalHeader = m_ui->tableView->horizontalHeader();
    const int kDefaultHorizontalSectionSize = 72;
    horizontalHeader->setDefaultSectionSize(kDefaultHorizontalSectionSize);
    horizontalHeader->hideSection(RulesTableModel::IdColumn);
    horizontalHeader->setSectionResizeMode(RulesTableModel::EventColumn, QHeaderView::Stretch);
    horizontalHeader->setSectionResizeMode(RulesTableModel::ActionColumn, QHeaderView::Stretch);
    horizontalHeader->setSectionResizeMode(RulesTableModel::EnabledStateColumn,
        QHeaderView::ResizeToContents);
    horizontalHeader->hideSection(RulesTableModel::CommentColumn);
    horizontalHeader->setSectionResizeMode(RulesTableModel::EditedStateColumn,
        QHeaderView::Fixed);
    horizontalHeader->moveSection(RulesTableModel::EditedStateColumn,
        RulesTableModel::EnabledStateColumn);

    connect(m_ui->tableView->selectionModel(), &QItemSelectionModel::currentChanged, this,
        [this](const QModelIndex& current)
        {
            m_displayedRule = current.isValid()
                ? m_rulesTableModel->rule(m_rulesFilterModel->mapToSource(current))
                : std::weak_ptr<SimplifiedRule>{};

            updateControlButtons();
            displayRule();
        });

    connect(m_ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            resetSelection();
            m_rulesFilterModel->setFilterRegularExpression(text);
        });

    connect(m_ui->tableView, &QAbstractItemView::clicked, this,
        [this](const QModelIndex &index)
        {
            if (index.column() != RulesTableModel::EnabledStateColumn)
                return;

            auto clickedRule = m_rulesTableModel->rule(m_rulesFilterModel->mapToSource(index)).lock();
            if (clickedRule)
                clickedRule->setEnabled(!clickedRule->enabled());
        });
}

void RulesDialog::updateReadOnlyState()
{
    m_ui->newRuleButton->setEnabled(!m_readOnly);
    m_ui->actionTypePicker->setEnabled(!m_readOnly);
    m_ui->eventTypePicker->setEnabled(!m_readOnly);

    for (auto paramsWidget: findChildren<ParamsWidget*>())
        paramsWidget->setReadOnly(m_readOnly);
}

void RulesDialog::updateControlButtons()
{
    m_ui->deleteRuleButton->setEnabled(m_readOnly ? false : !m_displayedRule.expired());

    const bool hasPermissions = accessController()->hasPowerUserPermissions();
    const bool hasChanges = m_rulesTableModel->hasChanges();
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasPermissions);
    m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasChanges && hasPermissions);
    m_ui->resetDefaultRulesButton->setEnabled(hasPermissions);
}

void RulesDialog::updateRuleEditorPanel()
{
    // Show rule editor if a rule selected, tip panel otherwise.
    constexpr auto kTipPanelIndex = 0;
    constexpr auto kEditorPanelIndex = 1;
    m_ui->panelsStackedWidget->setCurrentIndex(m_displayedRule.expired()
        ? kTipPanelIndex
        : kEditorPanelIndex);
}

void RulesDialog::createEventEditor(const vms::rules::ItemDescriptor& descriptor)
{
    if (m_eventEditorWidget)
        m_eventEditorWidget->deleteLater();

    m_eventEditorWidget = EventEditorFactory::createWidget(descriptor, context());
    if (!NX_ASSERT(m_eventEditorWidget))
        return;

    m_eventEditorWidget->setReadOnly(m_readOnly);

    m_ui->eventEditorContainerWidget->layout()->addWidget(m_eventEditorWidget);
}

void RulesDialog::createActionEditor(const vms::rules::ItemDescriptor& descriptor)
{
    if (m_actionEditorWidget)
        m_actionEditorWidget->deleteLater();

    m_actionEditorWidget = ActionEditorFactory::createWidget(descriptor, context());
    if (!NX_ASSERT(m_actionEditorWidget))
        return;

    m_actionEditorWidget->setReadOnly(m_readOnly);

    m_ui->actionEditorContainerWidget->layout()->addWidget(m_actionEditorWidget);
}

void RulesDialog::displayRule()
{
    updateRuleEditorPanel();

    auto rule = m_displayedRule.lock();
    if (!rule)
    {
        if (m_eventEditorWidget)
            m_eventEditorWidget->deleteLater();

        if (m_actionEditorWidget)
            m_actionEditorWidget->deleteLater();

        return;
    }

    displayEvent(rule);
    displayAction(rule);

    m_ui->footerWidget->setRule(m_displayedRule);
}

void RulesDialog::displayEvent(const std::shared_ptr<SimplifiedRule>& rule)
{
    {
        const QSignalBlocker blocker(m_ui->eventTypePicker);
        m_ui->eventTypePicker->setEventType(rule->eventType());
    }

    auto eventDescriptor = rule->eventDescriptor();
    if (!NX_ASSERT(eventDescriptor))
        return;

    if (!m_eventEditorWidget || m_eventEditorWidget->descriptor().id != eventDescriptor->id)
        createEventEditor(*eventDescriptor);

    if (m_eventEditorWidget)
        m_eventEditorWidget->setRule(rule);
}

void RulesDialog::displayAction(const std::shared_ptr<SimplifiedRule>& rule)
{
    {
        const QSignalBlocker blocker(m_ui->actionTypePicker);
        m_ui->actionTypePicker->setActionType(rule->actionType());
    }

    auto actionDescriptor = rule->actionDescriptor();
    if (!NX_ASSERT(actionDescriptor))
        return;

    if (!m_actionEditorWidget || m_actionEditorWidget->descriptor().id != actionDescriptor->id)
        createActionEditor(*actionDescriptor);

    if (m_actionEditorWidget)
        m_actionEditorWidget->setRule(rule);
}

void RulesDialog::resetFilter()
{
    m_ui->searchLineEdit->clear();
    m_rulesFilterModel->invalidate();
}

void RulesDialog::resetSelection()
{
    m_ui->tableView->clearSelection();
    m_ui->tableView->setCurrentIndex(QModelIndex{});
    displayRule();
}

void RulesDialog::onModelDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles)
{
    updateControlButtons();

    auto rule = m_displayedRule.lock();
    if (!rule)
        return;

    const auto displayedRuleRow = rule->modelIndex().row();
    const bool isDisplayedRuleAffected =
        displayedRuleRow >= topLeft.row() && displayedRuleRow <= bottomRight.row();

    if (!isDisplayedRuleAffected)
        return;

    if (roles.contains(RulesTableModel::FieldRole))
        displayRule();

    if (m_eventEditorWidget && m_actionEditorWidget)
    {
        m_eventEditorWidget->updateUi();
        m_actionEditorWidget->updateUi();
    }
}

void RulesDialog::applyChanges()
{
    const auto errorHandler =
        [this](const QString& error)
        {
            QnMessageBox::critical(this, tr("Failed to apply changes."), error);
        };

    m_rulesTableModel->applyChanges(errorHandler);
}

void RulesDialog::rejectChanges()
{
    m_rulesTableModel->rejectChanges();
}

void RulesDialog::resetToDefaults()
{
    if (!accessController()->hasPowerUserPermissions())
        return;

    QnMessageBox dialog(
        QnMessageBoxIcon::Question,
        tr("Restore all rules to default?"),
        tr("This action cannot be undone."),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        this);

    dialog.addCustomButton(
        QnMessageBoxCustomButton::Reset,
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Warning);

    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    const auto errorHandler =
        [this](const QString& error)
        {
            QnMessageBox::critical(this, tr("Failed to restore rules."), error);
        };

    m_rulesTableModel->resetToDefaults(errorHandler);
}

void RulesDialog::deleteCurrentRule()
{
    m_rulesTableModel->removeRule(
        m_rulesFilterModel->mapToSource(m_ui->tableView->selectionModel()->currentIndex()));
}

} // namespace nx::vms::client::desktop::rules
