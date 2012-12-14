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

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_rule(0),
    m_eventParameters(NULL),
    m_eventsTypesModel(new QStandardItemModel(this)),
    m_eventStatesModel(new QStandardItemModel(this)),
    m_actionTypesModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    ui->eventTypeComboBox->setModel(m_eventsTypesModel);
    ui->eventStatesComboBox->setModel(m_eventStatesModel);
    ui->actionTypeComboBox->setModel(m_actionTypesModel);

    connect(ui->eventTypeComboBox,      SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,    SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->actionTypeComboBox,     SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    connect(ui->expandButton, SIGNAL(clicked()), this, SIGNAL(expand()));
    connect(ui->resetButton,  SIGNAL(clicked()), this, SLOT(resetFromRule()));

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->applyButton,  SIGNAL(clicked()), this, SLOT(at_applyButton_clicked()));

    //TODO: init and reset from rule, then connect 'OnChanged', yep?
    initAnimations();
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

//TODO: refactor
void QnBusinessRuleWidget::initAnimations() {
    machine = new QStateMachine(this);
    QState *state1 = new QState(machine);
    state1->assignProperty(ui->editFrame, "maximumHeight", 1000); //TODO: magic const?

    QState *state2 = new QState(machine);
    state2->assignProperty(ui->editFrame, "maximumHeight", 0);

    machine->setInitialState(state2);

    QSignalTransition *transition1 = state1->addTransition(ui->expandButton, SIGNAL(clicked()), state2);
    transition1->addAnimation(new QPropertyAnimation(ui->editFrame, "maximumHeight"));

    QSignalTransition *transition2 = state2->addTransition(this, SIGNAL(expand()), state1);
    transition2->addAnimation(new QPropertyAnimation(ui->editFrame, "maximumHeight"));

    connect(machine, SIGNAL(stopped()), this, SLOT(updateSummary()));
    machine->start();
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

//TODO: 'needAnimate' flag is requested, or just do not animate during constructor and during first expansion
void QnBusinessRuleWidget::initEventParameters(BusinessEventType::Value eventType) {
    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);

    if (m_eventParameters) {
        ui->eventLayout->removeWidget(m_eventParameters);

        //TODO: animating opacity is not working on child widgets, need something else
        QPropertyAnimation *animation = new QPropertyAnimation(m_eventParameters, "windowOpacity", this);
        animation->setDuration(1500);
        animation->setStartValue(1.0);
        animation->setEndValue(0.0);
        group->addAnimation(animation);

        m_eventParameters->setVisible(false);
        //TODO: really it should be removed after animation transition
    }

    //TODO: reuse existing widget to use already set parameters
    //TODO: read parameters from rule if exist on first creating
    //TODO: common widget interface required to setup rule and read settings from it
    m_eventParameters = QnBusinessEventWidgetFactory::createWidget(eventType, this);
    if (m_eventParameters) {
        ui->eventLayout->addWidget(m_eventParameters);
        QPropertyAnimation *animation = new QPropertyAnimation(m_eventParameters, "windowOpacity", this);
        animation->setDuration(1500);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        group->addAnimation(animation);
    }

    //TODO: replace with state-machine
    group->start(QAbstractAnimation::DeleteWhenStopped);
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


    //TODO: do not lose data in the details widget
    //TODO: store data in persistent storage between changes?
}

void QnBusinessRuleWidget::initActionParameters(BusinessActionType::Value actionType) {

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
            .arg(BusinessEventType::toString(m_rule->getEventType()))
            .arg(ToggleState::toString(m_rule->getEventToggleState()))
            .arg(eventResource);

    QString actionSummary = QString(actionString)
            .arg(BusinessActionType::toString(m_rule->getActionType()))
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
        QModelIndexList eventTypeIdx = m_eventsTypesModel->match(m_eventsTypesModel->index(0, 0), Qt::UserRole, (int)m_rule->getEventType());
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        QModelIndexList stateIdx = m_eventStatesModel->match(m_eventStatesModel->index(0, 0), Qt::UserRole, (int)m_rule->getEventToggleState());
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());

        QModelIndexList actionTypeIdx = m_actionTypesModel->match(m_actionTypesModel->index(0, 0), Qt::UserRole, (int)m_rule->getActionType());
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());
    } else {
        ui->eventTypeComboBox->setCurrentIndex(0);
        ui->eventStatesComboBox->setCurrentIndex(0);
        ui->actionTypeComboBox->setCurrentIndex(0);
    }
}

// TODO: expansion from the outer module still not work
void QnBusinessRuleWidget::at_expandButton_clicked() {
  //  setExpanded(!m_expanded);
}

void QnBusinessRuleWidget::at_deleteButton_clicked() {
    //TODO: confirmation dialog, delete at the server
    //TODO: possible return just ID
    emit deleteConfirmed(m_rule);
}

void QnBusinessRuleWidget::at_applyButton_clicked() {
    QnBusinessEventRulePtr rule = m_rule ? m_rule : QnBusinessEventRulePtr(new QnBusinessEventRule);
    rule->setEventType(getCurrentEventType());
    rule->setActionType(getCurrentActionType());
    rule->setEventToggleState(getCurrentEventToggleState());
    //TODO: fill rule
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
    //TODO: setPossibleActions;
    // action parameters will be setup in at_action..indexChanged.
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_actionTypesModel->item(index)->data().toInt();
    BusinessActionType::Value val = (BusinessActionType::Value)typeIdx;

    initActionParameters(val);
}
