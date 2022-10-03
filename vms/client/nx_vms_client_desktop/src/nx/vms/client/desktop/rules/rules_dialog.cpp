// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_dialog.h"
#include "ui_rules_dialog.h"

#include <QtCore/QSortFilterProxyModel>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/rules/model_view/modification_mark_item_delegate.h>
#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/client/desktop/rules/params_widgets/editor_factory.h>
#include <nx/vms/client/desktop/rules/params_widgets/params_widget.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/indents.h>
#include <ui/common/palette.h>
#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop::rules {

RulesDialog::RulesDialog(QWidget* parent):
    QnSessionAwareButtonBoxDialog(parent),
    ui(new Ui::RulesDialog()),
    rulesTableModel(
        new RulesTableModel(appContext()->currentSystemContext()->vmsRulesEngine(), this)),
    rulesFilterModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    const auto buttonIconColor = QPalette().color(QPalette::BrightText);

    ui->newRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/arythmetic_plus.png"), buttonIconColor));
    ui->deleteRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/trash.png"), buttonIconColor));
    ui->deleteRuleButton->setEnabled(false);

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    const auto midlightColor = QPalette().color(QPalette::Midlight);
    setPaletteColor(ui->searchLineEdit, QPalette::PlaceholderText, midlightColor);

    connect(rulesTableModel, &RulesTableModel::dataChanged, this, &RulesDialog::onModelDataChanged);

    connect(ui->newRuleButton, &QPushButton::clicked, this,
        [this]
        {
            resetFilter();

            // Add a rule and select it.
            auto newRuleIndex = rulesTableModel->addRule();
            auto selectionModel = ui->tableView->selectionModel();
            selectionModel->setCurrentIndex(rulesFilterModel->mapFromSource(newRuleIndex),
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        });

    connect(ui->deleteRuleButton, &QPushButton::clicked, this,
        [this]
        {
            rulesTableModel->removeRule(
                rulesFilterModel->mapToSource(ui->tableView->selectionModel()->currentIndex()));
            updateControlButtons();
        });

    connect(ui->eventTypePicker, &EventTypePickerWidget::eventTypePicked, this,
        [this](const QString& eventType)
        {
            if (auto rule = displayedRule.lock())
                rule->setEventType(eventType);
        });

    connect(ui->eventTypePicker, &EventTypePickerWidget::eventContinuancePicked, this,
        [this](api::rules::State eventContinuance)
        {
            if (actionEditorWidget)
                actionEditorWidget->setInstant(eventContinuance == api::rules::State::instant);
        });

    connect(ui->actionTypePicker, &ActionTypePickerWidget::actionTypePicked, this,
        [this](const QString& actionType)
        {
            if (auto rule = displayedRule.lock())
                rule->setActionType(actionType);
        });

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
        [this]
        {
            applyChanges();
            updateControlButtons();
        });

    connect(ui->resetDefaultRulesButton, &QPushButton::clicked, this, &RulesDialog::resetToDefaults);

    setupRuleTableView();

    readOnly = false; //< TODO: Get real readonly state.
    updateReadOnlyState();
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
RulesDialog::~RulesDialog()
{
}

bool RulesDialog::tryClose(bool force)
{
    if (force)
    {
        rejectChanges();
        if (!isHidden())
            hide();

        return true;
    }

    if (!rulesTableModel->hasChanges())
        return true;

    const auto result = QnMessageBox::question(
        this,
        tr("Apply changes before exit?"),
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
    ui->searchLineEdit->setFocus();
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
    ui->tableView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(
        QnIndents(style::Metrics::kStandardPadding, style::Metrics::kStandardPadding)));

    auto scrollBar = new SnappedScrollBar(ui->tableHolder);
    ui->tableView->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseItemViewPaddingWhenVisible(true);

    rulesFilterModel->setSourceModel(rulesTableModel);
    rulesFilterModel->setFilterRole(RulesTableModel::FilterRole);
    rulesFilterModel->setDynamicSortFilter(true);
    rulesFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    rulesFilterModel->sort(RulesTableModel::EventColumn);

    ui->tableView->setModel(rulesFilterModel);
    ui->tableView->setItemDelegateForColumn(RulesTableModel::EditedStateColumn,
        new ModificationMarkItemDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(RulesTableModel::EnabledStateColumn,
        new SwitchItemDelegate(ui->tableView));
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto horizontalHeader = ui->tableView->horizontalHeader();
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

    const auto selectionModel = ui->tableView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentChanged, this,
        [this, selectionModel](const QModelIndex& current)
        {
            displayedRule = rulesTableModel->rule(rulesFilterModel->mapToSource(current));
            updateControlButtons();
            displayRule();
        });

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            resetSelection();
            rulesFilterModel->setFilterRegularExpression(text);
        });

    connect(ui->tableView, &QAbstractItemView::clicked, this,
        [this](const QModelIndex &index)
        {
            if (index.column() != RulesTableModel::EnabledStateColumn)
                return;

            auto clickedRule = rulesTableModel->rule(rulesFilterModel->mapToSource(index)).lock();
            if (clickedRule)
                clickedRule->setEnabled(!clickedRule->enabled());
        });
}

void RulesDialog::updateReadOnlyState()
{
    ui->newRuleButton->setEnabled(!readOnly);

    ui->actionTypePicker->setEnabled(!readOnly);
    ui->eventTypePicker->setEnabled(!readOnly);

    for (auto paramsWidget: findChildren<ParamsWidget*>())
        paramsWidget->setReadOnly(readOnly);
}

void RulesDialog::updateControlButtons()
{
    const bool hasSelection = ui->tableView->currentIndex().isValid();
    ui->deleteRuleButton->setEnabled(readOnly ? false : hasSelection);

    const bool hasPermissions = accessController()->hasAdminPermissions();
    const bool hasChanges = rulesTableModel->hasChanges();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasPermissions);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasChanges && hasPermissions);
    ui->resetDefaultRulesButton->setEnabled(hasPermissions);
}

void RulesDialog::updateRuleEditorPanel()
{
    const bool hasSelection = ui->tableView->currentIndex().isValid();
    // Show rule editor if a rule selected, tip panel otherwise.
    ui->panelsStackedWidget->setCurrentIndex(!hasSelection ? 0 : 1);
}

void RulesDialog::createEventEditor(const vms::rules::ItemDescriptor& descriptor)
{
    if (eventEditorWidget)
        eventEditorWidget->deleteLater();

    eventEditorWidget = EventEditorFactory::createWidget(descriptor, systemContext());
    if (!NX_ASSERT(eventEditorWidget))
        return;

    eventEditorWidget->setReadOnly(readOnly);

    ui->eventEditorContainerWidget->layout()->addWidget(eventEditorWidget);
}

void RulesDialog::createActionEditor(const vms::rules::ItemDescriptor& descriptor)
{
    if (actionEditorWidget)
        actionEditorWidget->deleteLater();

    actionEditorWidget = ActionEditorFactory::createWidget(descriptor, systemContext());
    if (!NX_ASSERT(actionEditorWidget))
        return;

    actionEditorWidget->setReadOnly(readOnly);
    actionEditorWidget->setInstant(
        ui->eventTypePicker->eventContinuance() == api::rules::State::instant);

    ui->actionEditorContainerWidget->layout()->addWidget(actionEditorWidget);
}

void RulesDialog::displayRule()
{
    updateRuleEditorPanel();

    auto rule = displayedRule.lock();
    if (!rule)
        return;

    displayEvent(*rule);
    displayAction(*rule);

    ui->footerWidget->setRule(displayedRule);
}

void RulesDialog::displayEvent(const SimplifiedRule& rule)
{
    {
        const QSignalBlocker blocker(ui->eventTypePicker);
        ui->eventTypePicker->setEventType(rule.eventType());
    }

    auto eventDescriptor = rule.eventDescriptor();
    if (!NX_ASSERT(eventDescriptor))
        return;

    if (!eventEditorWidget || eventEditorWidget->descriptor().id != eventDescriptor->id)
        createEventEditor(*eventDescriptor);

    if (eventEditorWidget)
        eventEditorWidget->setFields(rule.eventFields());
}

void RulesDialog::displayAction(const SimplifiedRule& rule)
{
    {
        const QSignalBlocker blocker(ui->actionTypePicker);
        ui->actionTypePicker->setActionType(rule.actionType());
    }

    auto actionDescriptor = rule.actionDescriptor();
    if (!NX_ASSERT(actionDescriptor))
        return;

    if (!actionEditorWidget || actionEditorWidget->descriptor().id != actionDescriptor->id)
        createActionEditor(*actionDescriptor);

    if (actionEditorWidget)
        actionEditorWidget->setFields(rule.actionFields());
}

void RulesDialog::resetFilter()
{
    ui->searchLineEdit->clear();
    rulesFilterModel->clear();
}

void RulesDialog::resetSelection()
{
    ui->tableView->clearSelection();
    ui->tableView->setCurrentIndex(QModelIndex{});
    displayRule();
}

void RulesDialog::onModelDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles)
{
    updateControlButtons();

    auto rule = displayedRule.lock();
    if (!rule)
        return;

    const auto displayedRuleRow = rule->modelIndex().row();
    const bool isDisplayedRuleAffected =
        displayedRuleRow >= topLeft.row() && displayedRuleRow <= bottomRight.row();

    if (!isDisplayedRuleAffected)
        return;

    if (roles.contains(RulesTableModel::FieldRole))
        displayRule();
}

void RulesDialog::applyChanges()
{
    const auto errorHandler =
        [this](const QString& error)
        {
            QnMessageBox::critical(this, tr("Failed to apply changes."), error);
        };

    rulesTableModel->applyChanges(errorHandler);
}

void RulesDialog::rejectChanges()
{
    rulesTableModel->rejectChanges();
}

void RulesDialog::resetToDefaults()
{
    if (!accessController()->hasAdminPermissions())
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

    rulesTableModel->resetToDefaults(errorHandler);
}

} // namespace nx::vms::client::desktop::rules
