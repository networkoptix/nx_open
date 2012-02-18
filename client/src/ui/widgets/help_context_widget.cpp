#include "help_context_widget.h"
#include "ui_help_context_widget.h"
#include <help/qncontext_help.h>

QnHelpContextWidget::QnHelpContextWidget(QnContextHelp *contextHelp, QWidget *parent):
    QWidget(parent),
    ui(new Ui::HelpContextWidget()),
    m_contextHelp(contextHelp)
{
    ui->setupUi(this);

    connect(contextHelp,    SIGNAL(helpContextChanged(QnContextHelp::ContextId)),   this,   SLOT(at_contextHelp_helpContextChanged(QnContextHelp::ContextId)));
    connect(ui->checkBox,   SIGNAL(toggled(bool)),                                  this,   SLOT(at_checkBox_toggled(bool)));
}

QnHelpContextWidget::~QnHelpContextWidget() {
    return;
}

void QnHelpContextWidget::at_contextHelp_helpContextChanged(QnContextHelp::ContextId id) {
    m_id = id;
    ui->label->setText(contextHelp()->text(id));
    ui->checkBox->setCheckState(contextHelp()->isNeedAutoShow(id) ? Qt::Unchecked : Qt::Checked);
}

void QnHelpContextWidget::at_checkBox_toggled(bool checked) {
    contextHelp()->setNeedAutoShow(m_id, !checked);
}