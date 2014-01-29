#include "abstract_ptz_dialog.h"

#include <QtWidgets/QLabel>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>

#include <ui/dialogs/ptz_synchronize_dialog.h>

QnAbstractPtzDialog::QnAbstractPtzDialog(const QnPtzControllerPtr &controller, QWidget *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    m_controller(controller),
    m_loaded(false)
{
    connect(this, &QnAbstractPtzDialog::synchronizeLater, this, &QnAbstractPtzDialog::synchronize, Qt::QueuedConnection);
    connect(m_controller, &QnAbstractPtzController::finished, this, &QnAbstractPtzDialog::at_controller_finished);

    synchronize();
}

QnAbstractPtzDialog::~QnAbstractPtzDialog() {
}

void QnAbstractPtzDialog::accept() {
    saveData();
    synchronize();
    base_type::accept();
}

void QnAbstractPtzDialog::synchronize() {
    if (!isVisible()) {
        emit synchronizeLater();
        return;
    }

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability)) {
        m_commands.enqueue(Qn::SynchronizePtzCommand);
        m_controller->synchronize(requiredFields());

        QScopedPointer<QnPtzSynchronizeDialog> dialog(new QnPtzSynchronizeDialog(this));
        connect(this, SIGNAL(synchronized()), dialog, SLOT(accept()));
        dialog->exec();
    } else {
        m_commands.clear(); //we will not wait for other commands
        m_commands.enqueue(Qn::SynchronizePtzCommand);

        QnPtzData data;
        m_controller->getData(requiredFields(), &data);
        at_controller_finished(Qn::SynchronizePtzCommand, QVariant::fromValue(data));
    }
}

bool QnAbstractPtzDialog::activatePreset(const QString &presetId, qreal speed) {
    return m_controller->activatePreset(presetId, speed);
}

bool QnAbstractPtzDialog::createPreset(const QnPtzPreset &preset) {
    m_commands.enqueue(Qn::CreatePresetPtzCommand);
    return m_controller->createPreset(preset);
}

bool QnAbstractPtzDialog::updatePreset(const QnPtzPreset &preset) {
    m_commands.enqueue(Qn::UpdatePresetPtzCommand);
    return m_controller->updatePreset(preset);
}

bool QnAbstractPtzDialog::removePreset(const QString &presetId) {
    m_commands.enqueue(Qn::RemovePresetPtzCommand);
    return m_controller->removePreset(presetId);
}

bool QnAbstractPtzDialog::createTour(const QnPtzTour &tour) {
    m_commands.enqueue(Qn::CreateTourPtzCommand);
    return m_controller->createTour(tour);
}

bool QnAbstractPtzDialog::removeTour(const QString &tourId) {
    m_commands.enqueue(Qn::RemoveTourPtzCommand);
    return m_controller->removeTour(tourId);
}

void QnAbstractPtzDialog::at_controller_finished(Qn::PtzCommand command, const QVariant &data) {
    if (m_commands.isEmpty())
        return;

    //TODO: #GDM PTZ check against validity of data, show error message later if not valid.
    if (m_commands.first() != command)
        return;
    m_commands.dequeue();

    if (m_commands.isEmpty() && command == Qn::SynchronizePtzCommand) {
        if (!m_loaded)
            loadData(data.value<QnPtzData>());
        m_loaded = true;
        emit synchronized();
    }
}
