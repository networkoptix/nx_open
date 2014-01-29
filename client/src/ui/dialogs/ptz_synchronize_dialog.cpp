#include "ptz_synchronize_dialog.h"
#include "ui_ptz_synchronize_dialog.h"

QnPtzSynchronizeDialog::QnPtzSynchronizeDialog(QWidget *parent) :
    base_type(parent, Qt::SplashScreen),
    ui(new Ui::QnPtzSynchronizeDialog)
{
    ui->setupUi(this);
    //TODO: #GDM PTZ add an animated "Loading" picture...
}

QnPtzSynchronizeDialog::~QnPtzSynchronizeDialog() {}

