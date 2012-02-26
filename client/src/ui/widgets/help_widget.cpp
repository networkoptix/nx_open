#include "help_widget.h"
#include <QWheelEvent>
#include <help/qncontext_help.h>
#include "ui_help_widget.h"

QnHelpWidget::QnHelpWidget(QnContextHelp *contextHelp, QWidget *parent):
    QWidget(parent),
    ui(new Ui::HelpWidget()),
    m_contextHelp(contextHelp)
{
    ui->setupUi(this);

    connect(contextHelp,    SIGNAL(helpContextChanged(QnContextHelp::ContextId)),   this,   SLOT(at_contextHelp_helpContextChanged(QnContextHelp::ContextId)));
    connect(ui->checkBox,   SIGNAL(clicked(bool)),                                  this,   SLOT(at_checkBox_clicked(bool)));
}

QnHelpWidget::~QnHelpWidget() {
    return;
}

void QnHelpWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the help widget. */
}

void QnHelpWidget::at_contextHelp_helpContextChanged(QnContextHelp::ContextId id) {
    m_id = id;
    ui->textEdit->setText(contextHelp()->text(id));
    ui->checkBox->setCheckState(contextHelp()->isNeedAutoShow(id) ? Qt::Unchecked : Qt::Checked);

    if(contextHelp()->isNeedAutoShow(id))
        emit showRequested();
}

void QnHelpWidget::at_checkBox_clicked(bool checked) {
    contextHelp()->setNeedAutoShow(m_id, !checked);

    if(checked)
        emit hideRequested();
}