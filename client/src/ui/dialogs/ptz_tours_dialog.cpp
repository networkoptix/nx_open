#include "ptz_tours_dialog.h"
#include "ui_ptz_tours_dialog.h"

#include <core/ptz/abstract_ptz_controller.h>

QnPtzToursDialog::QnPtzToursDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnPtzToursDialog)
{
    ui->setupUi(this);
}

QnPtzToursDialog::~QnPtzToursDialog() {
}

const QnPtzControllerPtr& QnPtzToursDialog::ptzController() const {
    return m_controller;
}

void QnPtzToursDialog::setPtzController(const QnPtzControllerPtr &controller) {
    if(m_controller == controller)
        return;

    m_controller = controller;
    updateModel();
}


void QnPtzToursDialog::accept() {
    base_type::accept();
}

void QnPtzToursDialog::updateModel() {

}
