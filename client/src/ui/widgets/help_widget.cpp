#include "help_widget.h"
#include <QWheelEvent>
#include <help/qncontext_help.h>
#include "ui_help_widget.h"

#define QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN

QnHelpWidget::QnHelpWidget(QnContextHelp *contextHelp, QWidget *parent):
    QWidget(parent),
    ui(new Ui::HelpWidget()),
    m_contextHelp(contextHelp)
{
    ui->setupUi(this);

#ifdef QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN
    ui->checkBox->hide();
#endif

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
    ui->checkBox->setCheckState(contextHelp()->isAutoShowNeeded() ? Qt::Unchecked : Qt::Checked);

#ifdef QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN
    if(contextHelp()->isAutoShowNeeded())
        emit showRequested();
#endif
}

void QnHelpWidget::at_checkBox_clicked(bool checked) {
    contextHelp()->setAutoShowNeeded(!checked);

#ifdef QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN
    if(checked)
        emit hideRequested();
#endif
}