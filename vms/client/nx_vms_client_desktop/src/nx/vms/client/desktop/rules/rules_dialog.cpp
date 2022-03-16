// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_dialog.h"
#include "ui_rules_dialog.h"

#include <QtCore/QSortFilterProxyModel>

#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/rules/model_view/enabled_state_item_delegate.h>
#include <nx/vms/client/desktop/rules/model_view/modification_mark_item_delegate.h>
#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/client/desktop/rules/params_widgets/editor_factory.h>
#include <nx/vms/client/desktop/rules/params_widgets/params_widget.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/indents.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

RulesDialog::RulesDialog(QWidget* parent):
    QnSessionAwareButtonBoxDialog(parent),
    ui(new Ui::RulesDialog()),
    rulesTableModel(new RulesTableModel(this)),
    rulesFilterModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    const auto buttonIconColor = QPalette().color(QPalette::BrightText);

    ui->newRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/arythmetic_plus.png"), buttonIconColor));
    ui->deleteRuleButton->setIcon(Skin::colorize(
        qnSkin->pixmap("text_buttons/trash.png"), buttonIconColor));
    ui->deleteRuleButton->setEnabled(false);

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
        });

    connect(ui->eventTypePicker, &EventTypePickerWidget::eventTypePicked, this,
        [this](const QString& eventType)
        {
            if (auto rule = displayedRule.lock())
                rule->setEventType(eventType);
        });

    connect(ui->eventTypePicker, &EventTypePickerWidget::eventContinuancePicked, this,
        [this](api::rules::EventInfo::State eventContinuance)
        {
            if (actionEditorWidget)
                actionEditorWidget->setInstant(eventContinuance == api::rules::EventInfo::State::instant);
        });

    connect(ui->actionTypePicker, &ActionTypePickerWidget::actionTypePicked, this,
        [this](const QString& actionType)
        {
            if (auto rule = displayedRule.lock())
                rule->setActionType(actionType);
        });

    setupRuleTableView();

    readOnly = false; //< TODO: Get real readonly state.
    updateReadOnlyState();
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
RulesDialog::~RulesDialog()
{
}

void RulesDialog::showEvent(QShowEvent* event)
{
    displayRule();

    QnSessionAwareButtonBoxDialog::showEvent(event);
}

void RulesDialog::closeEvent(QCloseEvent* event)
{
    resetFilter();
    resetSelection();

    QnSessionAwareButtonBoxDialog::closeEvent(event);
}

void RulesDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    const auto errorHandler =
        [this](const QString& error)
        {
            QnMessageBox::critical(this, error);
        };

    switch(button)
    {
        case QDialogButtonBox::Apply:
        case QDialogButtonBox::Ok:
            rulesTableModel->applyChanges(errorHandler);
            break;
        case QDialogButtonBox::Cancel:
            rulesTableModel->rejectChanges();
            break;
    }

    resetSelection();
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

    ui->tableView->setModel(rulesFilterModel);
    ui->tableView->setItemDelegateForColumn(RulesTableModel::EditedStateColumn,
        new ModificationMarkItemDelegate(ui->tableView));
    ui->tableView->setItemDelegateForColumn(RulesTableModel::EnabledStateColumn,
        new EnabledStateItemDelegate(ui->tableView));

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
            displayRule();
        });

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            resetSelection();
            rulesFilterModel->setFilterRegularExpression(text);
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

void RulesDialog::showTipPanel(bool show)
{
    ui->panelsStackedWidget->setCurrentIndex(show ? 0 : 1);
}

void RulesDialog::createEventEditor(const vms::rules::ItemDescriptor& descriptor)
{
    if (eventEditorWidget)
        eventEditorWidget->deleteLater();

    eventEditorWidget = EventEditorFactory::createWidget(descriptor);
    if (!NX_ASSERT(eventEditorWidget))
        return;

    connect(
        eventEditorWidget,
        &ParamsWidget::edited,
        this,
        &RulesDialog::updateRule);

    eventEditorWidget->setReadOnly(readOnly);

    ui->eventEditorContainerWidget->layout()->addWidget(eventEditorWidget);
}

void RulesDialog::createActionEditor(const vms::rules::ItemDescriptor& descriptor)
{
    if (actionEditorWidget)
        actionEditorWidget->deleteLater();

    actionEditorWidget = ActionEditorFactory::createWidget(descriptor);
    if (!NX_ASSERT(actionEditorWidget))
        return;

    connect(
        actionEditorWidget,
        &ParamsWidget::edited,
        this,
        &RulesDialog::updateRule);

    actionEditorWidget->setReadOnly(readOnly);
    actionEditorWidget->setInstant(
        ui->eventTypePicker->eventContinuance() == api::rules::EventInfo::State::instant);

    ui->actionEditorContainerWidget->layout()->addWidget(actionEditorWidget);
}

void RulesDialog::displayRule()
{
    auto rule = displayedRule.lock();
    if (!rule)
    {
        ui->deleteRuleButton->setEnabled(false);
        showTipPanel(true);
        return;
    }

    ui->deleteRuleButton->setEnabled(readOnly ? false : true);

    // Hide tip panel.
    showTipPanel(false);

    displayEvent(*rule);
    displayAction(*rule);
    displayComment(*rule);
}

void RulesDialog::displayEvent(const RulesTableModel::SimplifiedRule& rule)
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

void RulesDialog::displayAction(const RulesTableModel::SimplifiedRule& rule)
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

void RulesDialog::displayComment(const RulesTableModel::SimplifiedRule& rule)
{
    ui->footerWidget->setComment(rule.comment());
}

void RulesDialog::resetFilter()
{
    ui->searchLineEdit->clear();
    rulesFilterModel->clear();
}

void RulesDialog::resetSelection()
{
    ui->tableView->selectionModel()->clear();
    displayedRule.reset();
}

void RulesDialog::updateRule()
{
    if (auto rule = displayedRule.lock())
        rule->update();
}

void RulesDialog::onModelDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles)
{
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

} // namespace nx::vms::client::desktop::rules
