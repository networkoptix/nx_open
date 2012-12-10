#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>


QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget),
    m_expanded(false)
{
    ui->setupUi(this);

   // connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(at_expandButton_clicked()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));

    machine = new QStateMachine(this);
    QState *state1 = new QState(machine);
    state1->assignProperty(ui->editFrame, "maximumHeight", 200);
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

    updateDisplay();
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
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

// Handlers

void QnBusinessRuleWidget::updateDisplay() {
    ui->editFrame->setVisible(m_expanded);
}

void QnBusinessRuleWidget::at_expandButton_clicked() {
    setExpanded(!m_expanded);
}

void QnBusinessRuleWidget::at_deleteButton_clicked() {
    QPropertyAnimation *animation = new QPropertyAnimation(ui->editFrame, "maximumHeight");
    animation->setDuration(500);
    animation->setStartValue(ui->editFrame->height());
    animation->setEndValue(0);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
    //ui->editFrame->setMaximumHeight(1);
}
