#include "update_dialog.h"
#include "ui_update_dialog.h"

#include <ui/widgets/server_updates_widget.h>

QnUpdateDialog::QnUpdateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnUpdateDialog)
{
    ui->setupUi(this);
    m_updatesWidget = new QnServerUpdatesWidget(this);
}

QnUpdateDialog::~QnUpdateDialog() {}
