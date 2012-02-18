#include "help_context_widget.h"
#include "ui_help_context_widget.h"
#include <help/qncontext_help.h>

QnHelpContextWidget::QnHelpContextWidget(QnContextHelp *contextHelp, QWidget *parent):
    QWidget(parent),
    ui(new Ui::HelpContextWidget()),
    m_contextHelp(contextHelp)
{
    ui->setupUi(this);

    setMinimumWidth(180);
    setMaximumWidth(300);

    connect(contextHelp, SIGNAL(helpContextChanged(QnContextHelp::ContextId)), this, SLOT(at_contextHelp_helpContextChanged(QnContextHelp::ContextId)));
}

QnHelpContextWidget::~QnHelpContextWidget() {
    return;
}

void QnHelpContextWidget::at_contextHelp_helpContextChanged(QnContextHelp::ContextId id) {
    
}

