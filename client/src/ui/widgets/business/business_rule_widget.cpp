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

QnBusinessRuleWidget::QnBusinessRuleWidget(QnBusinessEventRulePtr rule, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_expanded(false),
    m_rule(rule),
    m_eventParameters(NULL)
{
    ui->setupUi(this);

    m_eventsTypesModel = new QStandardItemModel(this);
    ui->eventTypeComboBox->setModel(m_eventsTypesModel);
    m_eventStatesModel = new QStandardItemModel(this);
    ui->eventStatesComboBox->setModel(m_eventStatesModel);

    initEventTypes();
    connect(ui->eventTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));

   // connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));

    initAnimations();
    updateDisplay();
    updateSelection();
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
}

//TODO: refactor
void QnBusinessRuleWidget::initAnimations() {
    machine = new QStateMachine(this);
    QState *state1 = new QState(machine);
    state1->assignProperty(ui->editFrame, "maximumHeight", 1000); //TODO: magic const?
    state1->assignProperty(ui->editFrame, "visible", true);

    QState *state2 = new QState(machine);
    //state2->assignProperty(ui->editFrame, "visible", false);
    state2->assignProperty(ui->editFrame, "maximumHeight", 0);

    machine->setInitialState(state2);

    QSignalTransition *transition1 = state1->addTransition(ui->expandButton, SIGNAL(clicked()), state2);
    transition1->addAnimation(new QPropertyAnimation(ui->editFrame, "maximumHeight"));

    QSignalTransition *transition2 = state2->addTransition(ui->expandButton, SIGNAL(clicked()), state1);
    transition2->addAnimation(new QPropertyAnimation(ui->editFrame, "maximumHeight"));

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
    values << ToggleState::Any << ToggleState::On << ToggleState::Off;
    if (BusinessEventType::hasToggleState(eventType))
        values << ToggleState::NotDefined;

    foreach (ToggleState::Value val, values) {
        QStandardItem *item = new QStandardItem(QLatin1String(ToggleState::toString(val)));
        item->setData(val);

        QList<QStandardItem *> row;
        row << item;
        m_eventStatesModel->appendRow(row);
    }
}

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
    group->start();
}

void QnBusinessRuleWidget::initActionTypes() {
    //TODO: will be called from event_type_changed
    //TODO: do not lose data in the details widget if action is possible
    //TODO: store data in persistent storage between changes?
}

/*
void QnBusinessRuleWidget::setExpanded(bool expanded) {
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    updateDisplay();
}

bool QnBusinessRuleWidget::getExpanded() {
    return m_expanded;
}
*/

// Handlers

void QnBusinessRuleWidget::updateDisplay() {
    ui->editFrame->setVisible(m_expanded);
}

void QnBusinessRuleWidget::updateSelection() {
    ui->eventTypeComboBox->setCurrentIndex((int)m_rule->getEventType());
}

// TODO: expansion from the outer module still not work
void QnBusinessRuleWidget::at_expandButton_clicked() {
  //  setExpanded(!m_expanded);
}

void QnBusinessRuleWidget::at_deleteButton_clicked() {
    //TODO: confirmation dialog, delete at the server
}

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    int typeIdx = m_eventsTypesModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;

    initEventStates(val);
    initEventParameters(val);

    //TODO: setPossibleActions;
    // action parameters will be setup in at_action..indexChanged.
}
