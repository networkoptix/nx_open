#include "help_widget.h"
#include <QWheelEvent>
#include <help/context_help.h>
#include "ui_help_widget.h"

#define QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN

QnHelpWidget::QnHelpWidget(QnHelpHandler *contextHelp, QWidget *parent):
    QWidget(parent),
    ui(new Ui::HelpWidget()),
    m_contextHelp(contextHelp)
{
    ui->setupUi(this);

#ifdef QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN
    ui->checkBox->hide();
#endif

    /* This is needed so that control's context menu is not embedded into the scene. */
    ui->textEdit->setWindowFlags(ui->textEdit->windowFlags() | Qt::BypassGraphicsProxyWidget);

    connect(contextHelp,    SIGNAL(helpContextChanged(QnHelpHandler::ContextId)),   this,   SLOT(at_contextHelp_helpContextChanged(QnHelpHandler::ContextId)));
    connect(ui->checkBox,   SIGNAL(clicked(bool)),                                  this,   SLOT(at_checkBox_clicked(bool)));
}

QnHelpWidget::~QnHelpWidget() {
    return;
}

void QnHelpWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the help widget. */
}

void QnHelpWidget::at_contextHelp_helpContextChanged(int id) {
    /*m_id = id;
    ui->textEdit->setText(contextHelp()->text(id));
    ui->checkBox->setCheckState(contextHelp()->isAutoShowNeeded() ? Qt::Unchecked : Qt::Checked);

#ifndef QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN
    if(contextHelp()->isAutoShowNeeded())
        emit showRequested();
#endif*/
}

void QnHelpWidget::at_checkBox_clicked(bool checked) {
    /*contextHelp()->setAutoShowNeeded(!checked);

#ifndef QN_HELP_WIDGET_NO_DONT_SHOW_AGAIN
    if(checked)
        emit hideRequested();
#endif*/
}