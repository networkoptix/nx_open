#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>

#include <events/abstract_business_event.h>
#include <events/business_logic_common.h>

QnBusinessRuleWidget::QnBusinessRuleWidget(QnBusinessEventRulePtr rule, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_expanded(false),
    m_rule(rule)
{
    ui->setupUi(this);

    m_eventsModel = new QStandardItemModel(this);
    ui->eventTypeComboBox->setModel(m_eventsModel);
    initEventTypes();
    connect(ui->eventTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));

   // connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));

    initAnimations();
    updateDisplay();
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

void QnBusinessRuleWidget::setExpanded(bool expanded) {
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    updateDisplay();
}

bool QnBusinessRuleWidget::getExpanded() {
    return m_expanded;
}

void QnBusinessRuleWidget::initEventTypes() {
    m_eventsModel->clear();
    for (int i = BusinessEventType::BE_FirstType; i <= BusinessEventType::BE_LastType; i++) {
        BusinessEventType::Value val = (BusinessEventType::Value)i;

        QList<QStandardItem *> row;
        QStandardItem *item = new QStandardItem(BusinessEventType::toString(val));
        item->setData(val);
        row << item;
        m_eventsModel->appendRow(row);
    }
}

void QnBusinessRuleWidget::initActionTypes() {
    //TODO: will be called from event_type_changed
    //TODO: do not lose data in the details widget if action is possible
    //TODO: store data in persistent storage between changes?
}

// Handlers

void QnBusinessRuleWidget::updateDisplay() {
    ui->editFrame->setVisible(m_expanded);
}

// TODO: expansion from the outer module still not work
void QnBusinessRuleWidget::at_expandButton_clicked() {
    setExpanded(!m_expanded);
}

void QnBusinessRuleWidget::at_deleteButton_clicked() {
    //TODO: confirmation dialog, delete at the server
}

void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    QModelIndex idx= m_eventsModel->index(index, 0);
    qDebug() << m_eventsModel->itemData(idx);
    int typeIdx = m_eventsModel->item(index)->data().toInt();
    BusinessEventType::Value val = (BusinessEventType::Value)typeIdx;
    //TODO: setPossibleActions; setEventParameters;
    // action parameters will be setup in at_action..indexChanged.
}
