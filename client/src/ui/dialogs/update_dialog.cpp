#include "update_dialog.h"
#include "ui_update_dialog.h"

#include <ui/widgets/server_updates_widget.h>

QnUpdateDialog::QnUpdateDialog(QnWorkbenchContext *context, QWidget *parent) :
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::QnUpdateDialog)
{
    ui->setupUi(this);
    m_updatesWidget = new QnServerUpdatesWidget(this);
    ui->layout->addWidget(m_updatesWidget);
}

QnUpdateDialog::~QnUpdateDialog() {}

QnMediaServerUpdateTool *QnUpdateDialog::updateTool() const {
    return m_updatesWidget->updateTool();
}
