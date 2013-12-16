#include "abstract_ptz_dialog.h"

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>

QnAbstractPtzDialog::QnAbstractPtzDialog(QWidget *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags)
{
    connect(this, SIGNAL(synchronizeLater()), this, SLOT(synchronize()), Qt::QueuedConnection);
}

QnAbstractPtzDialog::~QnAbstractPtzDialog() {
}

const QnPtzControllerPtr& QnAbstractPtzDialog::ptzController() const {
    return m_controller;
}

void QnAbstractPtzDialog::setPtzController(const QnPtzControllerPtr &controller) {
    if(m_controller == controller)
        return;

    if (m_controller)
        disconnect(m_controller, NULL, this, NULL);

    m_controller = controller;

    if(m_controller)
        connect(m_controller, &QnAbstractPtzController::finished, this, &QnAbstractPtzDialog::at_controller_finished);

    synchronize();
}

void QnAbstractPtzDialog::accept() {
    if (m_controller) {
        saveData();
        disconnect(m_controller, NULL, this, NULL); //we do not want additional handling on syncronized() --gdm
        synchronize();
    }

    base_type::accept();
}

void QnAbstractPtzDialog::synchronize() {
    if (!m_controller)
        return;

    if (!isVisible()) {
        emit synchronizeLater();
        return;
    }

    QDialog* syncronizeDialog = new QDialog(this, Qt::SplashScreen);
    QLabel *label = new QLabel(tr("Syncronizing..."), syncronizeDialog);
    QLayout* layout = new QHBoxLayout(syncronizeDialog);
    layout->addWidget(label);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    syncronizeDialog->setLayout(layout);
    syncronizeDialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    syncronizeDialog->setModal(true);
    syncronizeDialog->setGeometry(this->geometry().center().x(), this->geometry().center().y(), 1, 1); //TODO: #GDM make this on showEvent() and geometryChanged()
    connect(m_controller, SIGNAL(synchronized(QnPtzData)), syncronizeDialog, SLOT(accept()));
    m_controller->synchronize(requiredFields());
    syncronizeDialog->exec();
}

void QnAbstractPtzDialog::at_controller_finished(Qn::PtzCommand command, const QVariant &data) {
    //loadData(data);
}
