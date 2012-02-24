#include "help_widget.h"
#include "ui_help_widget.h"
#include <help/qncontext_help.h>

QnHelpWidget::QnHelpWidget(QnContextHelp *contextHelp, QWidget *parent):
    QWidget(parent),
    ui(new Ui::HelpWidget()),
    m_contextHelp(contextHelp)
{
    ui->setupUi(this);

    connect(contextHelp,    SIGNAL(helpContextChanged(QnContextHelp::ContextId)),   this,   SLOT(at_contextHelp_helpContextChanged(QnContextHelp::ContextId)));
    connect(ui->checkBox,   SIGNAL(toggled(bool)),                                  this,   SLOT(at_checkBox_toggled(bool)));
}

QnHelpWidget::~QnHelpWidget() {
    return;
}

void QnHelpWidget::at_contextHelp_helpContextChanged(QnContextHelp::ContextId id) {
    m_id = id;
    ui->label->setText(contextHelp()->text(id));
    ui->checkBox->setCheckState(contextHelp()->isNeedAutoShow(id) ? Qt::Unchecked : Qt::Checked);
}

void QnHelpWidget::at_checkBox_toggled(bool checked) {
    contextHelp()->setNeedAutoShow(m_id, !checked);
}