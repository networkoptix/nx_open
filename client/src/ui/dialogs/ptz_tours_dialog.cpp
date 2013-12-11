#include "ptz_tours_dialog.h"
#include "ui_ptz_tours_dialog.h"

QnPtzToursDialog::QnPtzToursDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnPtzToursDialog)
{
    ui->setupUi(this);
}

QnPtzToursDialog::~QnPtzToursDialog() {
}
