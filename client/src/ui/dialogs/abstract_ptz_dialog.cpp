#include "abstract_ptz_dialog.h"

#include <QtCore/QEventLoop>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>

#include <ui/widgets/dialog_button_box.h>

QnAbstractPtzDialog::QnAbstractPtzDialog(const QnPtzControllerPtr &controller, QWidget *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    m_controller(controller),
    m_loaded(false)
{
    connect(this, &QnAbstractPtzDialog::synchronizeLater, this, &QnAbstractPtzDialog::synchronize, Qt::QueuedConnection);
    connect(m_controller, &QnAbstractPtzController::finished, this, &QnAbstractPtzDialog::at_controller_finished);

    synchronize(tr("Loading..."));
}

QnAbstractPtzDialog::~QnAbstractPtzDialog() {
}

void QnAbstractPtzDialog::accept() {
    saveData();
    synchronize(tr("Saving..."));
    base_type::accept();
}

void QnAbstractPtzDialog::synchronize(const QString &title) {
    if (!isVisible()) {
        emit synchronizeLater(title);
        return;
    }

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability)) {
        m_commands.enqueue(Qn::SynchronizePtzCommand);
        m_controller->synchronize(requiredFields());

        QEventLoop loop;
        connect(this,            SIGNAL(synchronized()),   &loop, SLOT(quit()));

        QList<QWidget*> disabled;
        QLayout* dialogLayout = this->layout();

        for (int i = 0; i < dialogLayout->count(); i++) {
            QWidget* widget = dialogLayout->itemAt(i)->widget();
            if (!widget)
                continue;
            if (QnDialogButtonBox* buttonBox = dynamic_cast<QnDialogButtonBox*>(widget)) {
                buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
                disabled << buttonBox->button(QDialogButtonBox::Ok);
                connect(buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked), &loop, SLOT(quit()));
                buttonBox->showProgress(title);
                connect(this, SIGNAL(synchronized()), buttonBox, SLOT(hideProgress()));
            } else
                disabled << widget;
        }
        foreach (QWidget* widget, disabled)
            widget->setEnabled(false);

        loop.exec();

        foreach (QWidget* widget, disabled)
            widget->setEnabled(true);

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
