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

const QnPtzControllerPtr &QnAbstractPtzDialog::ptzController() const {
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

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability)) {
        QDialog *synchronizeDialog = new QDialog(this, Qt::SplashScreen);
        QLabel *label = new QLabel(tr("Synchronizing..."), synchronizeDialog);
        QLayout *layout = new QHBoxLayout(synchronizeDialog);
        layout->addWidget(label);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        synchronizeDialog->setLayout(layout);
        synchronizeDialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        synchronizeDialog->setModal(true);
        synchronizeDialog->setGeometry(this->geometry().center().x(), this->geometry().center().y(), 1, 1); //TODO: #GDM make this on showEvent() and geometryChanged()
        connect(m_controller, &QnAbstractPtzController::finished, synchronizeDialog, &QDialog::accept); // TODO: #Elric wrong, we need only one specific signal
        m_controller->synchronize(requiredFields());
        synchronizeDialog->exec();
    } else {
        QnPtzData data;
        m_controller->getData(requiredFields(), &data);
        at_controller_finished(Qn::SynchronizePtzCommand, QVariant::fromValue(data));
    }
}

void QnAbstractPtzDialog::at_controller_finished(Qn::PtzCommand command, const QVariant &data) {
    if(command == Qn::SynchronizePtzCommand)
        loadData(data.value<QnPtzData>());
}
