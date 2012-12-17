#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>

#include <events/abstract_business_event.h>

#include <ui/widgets/business/business_event_widget_factory.h>
#include <ui/widgets/business/business_action_widget_factory.h>

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_expanded(false),
    m_rule(0),
    m_eventParameters(NULL),
    m_actionParameters(NULL),
    m_eventsTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    ui->eventTypeComboBox->setModel(m_eventsTypesModel);
    ui->eventStatesComboBox->setModel(m_eventStatesModel);
    ui->actionTypeComboBox->setModel(m_actionTypesModel);

    QPalette pal = ui->editFrame->palette();
    QColor bgcolor = pal.color(QPalette::Window);
    pal.setColor(QPalette::Window, QColor(bgcolor.red() + 10, bgcolor.green() + 10, bgcolor.blue() + 10));
    ui->editFrame->setPalette(pal);
    ui->editFrame->setVisible(m_expanded);

    connect(ui->eventTypeComboBox,      SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,    SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->actionTypeComboBox,     SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));
    connect(ui->resetButton,  SIGNAL(clicked()), this, SLOT(resetFromRule()));

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->applyButton,  SIGNAL(clicked()), this, SLOT(at_applyButton_clicked()));
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));

    initEventTypes();
    updateDisplay();
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
}

void QnBusinessRuleWidget::setRule(QnBusinessEventRulePtr rule) {
    m_rule = rule;
    updateDisplay();
    resetFromRule();
}

QnBusinessEventRulePtr QnBusinessRuleWidget::getRule() const {
    return m_rule;
}

void QnBusinessRuleWidget::setExpanded(bool expanded) {
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    ui->editFrame->setVisible(expanded);

    QIcon icon;
    if (m_expanded)
        icon.addFile(QString::fromUtf8(":/skin/titlebar/unfullscreen_hovered.png"));
    else
        icon.addFile(QString::fromUtf8(":/skin/titlebar/fullscreen_hovered.png"));
    ui->expandButton->setIcon(icon);
    if (expanded)
        resetFromRule();
}

bool QnBusinessRuleWidget::expanded() const {
    return m_expanded;
}

void QnBusinessRuleWidget::initEventTypes() {
    m_eventsTypesModel->clear();
    for (int i = BusinessEventType::BE_FirstType; i <= BusinessEventType::BE_LastType; i++) {
        BusinessEventType::Value val = (BusinessEventType::Value)i;

        QStandardItem *item = new QStandardItem(BusinessEventType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventsTypesModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initEventStates(BusinessEventType::Value eventType) {
    m_eventStatesModel->clear();

    QList<ToggleState::Value> values;
    values << ToggleState::NotDefined;
    if (BusinessEventType::hasToggleState(eventType))
        values << ToggleState::Any << ToggleState::On << ToggleState::Off;

    foreach (ToggleState::Value val, values) {
        QStandardItem *item = new QStandardItem(ToggleState::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initEventParameters(BusinessEventType::Value eventType) {
    if (m_eventParameters) {
        ui->eventLayout->removeWidget(m_eventParameters);
        m_eventParameters->setVisible(false);
    }

    if (m_eventWidgetsByType.contains(eventType)) {
        m_eventParameters = m_eventWidgetsByType.find(eventType).value();
    } else {
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(eventType, this);
        m_eventWidgetsByType[eventType] = m_eventParameters;
    }
    if (m_eventParameters) {
        ui->eventLayout->addWidget(m_eventParameters);
        m_eventParameters->setVisible(true);
    }
}

void QnBusinessRuleWidget::initActionTypes(ToggleState::Value eventState) {

    m_actionTypesModel->clear();

    // what type of actions to show: prolonged or instant
    bool instantActionsFilter = (eventState != ToggleState::NotDefined) || (!BusinessEventType::hasToggleState(getCurrentEventType()));

    for (int i = BusinessActionType::BA_FirstType; i <= BusinessActionType::BA_LastType; i++) {
        BusinessActionType::Value val = (BusinessActionType::Value)i;

        if (BusinessActionType::hasToggleState(val) == instantActionsFilter)
            continue;

        QStandardItem *item = new QStandardItem(BusinessActionType::toString(val));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_actionTypesModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initActionParameters(BusinessActionType::Value actionType) {
    if (m_actionParameters) {
        ui->actionLayout->removeWidget(m_actionParameters);
        m_actionParameters->setVisible(false);
    }

    if (m_actionWidgetsByType.contains(actionType)) {
        m_actionParameters = m_actionWidgetsByType.find(actionType).value();
    } else {
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(actionType, this);
        m_actionWidgetsByType[actionType] = m_actionParameters;
    }

    if (m_actionParameters) {
        ui->actionLayout->addWidget(m_actionParameters);
        m_actionParameters->setVisible(true);
    }
}

BusinessEventType::Value QnBusinessRuleWidget::getCurrentEventType() const {
    int typeIdx = m_eventsTypesModel->item(ui->eventTypeComboBox->currentIndex())->data().toInt();
    return (BusinessEventType::Value)typeIdx;
}

BusinessActionType::Value QnBusinessRuleWidget::getCurrentActionType() const {
    int typeIdx = m_actionTypesModel->item(ui->actionTypeComboBox->currentIndex())->data().toInt();
    return (BusinessActionType::Value)typeIdx;
}

ToggleState::Value QnBusinessRuleWidget::getCurrentEventToggleState() const {
    int typeIdx = m_eventStatesModel->item(ui->eventStatesComboBox->currentIndex())->data().toInt();
    return (ToggleState::Value)typeIdx;
}

// Handlers

void QnBusinessRuleWidget::updateDisplay() {
    ui->summaryFrame->setVisible(m_rule);
    ui->deleteButton->setVisible(m_rule);
    if (m_rule)
        updateSummary();
}

void QnBusinessRuleWidget::updateSummary() {
    if (!m_rule)
        return;

    QLatin1String formatString("<html><head/><body><p>%1: %2</p></body></html>");

    QLatin1String eventString("When %1 %2 at %3");
    QLatin1String actionString("Do %1 at %2");

    QLatin1String cameraString("<img src=\":/skin/tree/camera.png\" width=\"16\" height=\"16\"/>"\
            "<span style=\" font-style:italic;\">%1</span>");

    QString eventResource = QString(cameraString)
            .arg(QLatin1String("Camera_name"));

    QString actionResource = QString(cameraString)
            .arg(QLatin1String("Camera_name"));

    QString eventSummary = QString(eventString)
            .arg(BusinessEventType::toString(m_rule->eventType()))
            .arg(ToggleState::toString(m_rule->getEventToggleState()))
            .arg(eventResource);

    QString actionSummary = QString(actionString)
            .arg(BusinessActionType::toString(m_rule->actionType()))
            .arg(actionResource);

    //TODO: load details description from sub-widgets
    QString summary = QString(formatString)
            .arg(eventSummary)
            .arg(actionSummary);


    ui->summaryLabel->setText(summary);
    /*

           %1 On motion at
           %2 start recording at
           %3 <span style=" text-decoration: underline;">Low Quality</span>, <span style=" text-decoration: underline;">25FPS</span>, <span style=" text-decoration: underline;">10 sec pre</span>, <span style=" text-decoration: underline;">10 sec post</span>
    */


}

void QnBusinessRuleWidget::resetFromRule() {
    if (m_rule) {
        QModelIndexList eventTypeIdx = m_eventsTypesModel->match(m_eventsTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->eventType());
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        QModelIndexList stateIdx = m_eventStatesModel->match(m_eventStatesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->getEventToggleState());
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());

        QModelIndexList actionTypeIdx = m_actionTypesModel->match(m_actionTypesModel->index(0, 0), Qt::UserRole + 1, (int)m_rule->actionType());
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

        if (m_eventParameters)
            m_eventParameters->loadParameters(m_rule->eventParams());

        if (m_actionParameters)
            m_actionParameters->loadParameters(m_rule->actionParams());
    } else {
        ui->eventTypeComboBox->setCurrentIndex(0);
        ui->eventStatesComboBox->setCurrentIndex(0);
        ui->actionTypeComboBox->setCurrentIndex(0);
    }
}

void QnBusinessRuleWidget::at_expandButton_clicked() {
    setExpanded(!m_expanded);
}

void QnBusinessRuleWidget::at_deleteButton_clicked() {
    //TODO: confirmation dialog, delete at the server
    //TODO: possible return just ID
    emit deleteConfirmed(m_rule);
}

void QnBusinessRuleWidget::at_applyButton_clicked() {
    QnBusinessEventRulePtr rule = m_rule ? m_rule : QnBusinessEventRulePtr(new QnBusinessEventRule);

    rule->setEventType(getCurrentEventType());

    if (m_eventParameters)
        rule->setEventParams(m_eventParameters->parameters());
    rule->setEventToggleState(getCurrentEventToggleState());

    rule->setActionType(getCurrentActionType());
    if (m_actionParameters)
        rule->setActionParams(m_actionParameters->parameters());

    //TODO: fill resources
    emit apply(rule);
    updateSummary();
}

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventsTypesModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;

    //TODO: check isResourceRequired()

    initEventStates(val);
    initEventParameters(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventStatesModel->item(index)->data().toInt();
    ToggleState::Value val = (ToggleState::Value)typeIdx;

    initActionTypes(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_actionTypesModel->item(index)->data().toInt();
    BusinessActionType::Value val = (BusinessActionType::Value)typeIdx;

    initActionParameters(val);
}
