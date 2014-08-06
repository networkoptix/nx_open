#include "abstract_ptz_dialog.h"

#include <QtCore/QEventLoop>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QApplication>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_data.h>

#include <ui/widgets/dialog_button_box.h>

QnAbstractPtzDialog::QnAbstractPtzDialog(QWidget *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    m_loaded(false)
{
    connect(this, &QnAbstractPtzDialog::synchronizeLater, this, &QnAbstractPtzDialog::synchronize, Qt::QueuedConnection);
}

QnAbstractPtzDialog::~QnAbstractPtzDialog() {
}

void QnAbstractPtzDialog::accept() {
    saveChanges();
    base_type::accept();
}

QnPtzControllerPtr QnAbstractPtzDialog::controller() const {
    return m_controller;
}

void QnAbstractPtzDialog::setController(const QnPtzControllerPtr &controller) {
    if (m_controller == controller)
        return;

    if (m_controller) {
        disconnect(m_controller, NULL, this, NULL);
    }

    m_commands.clear();
    m_loaded = false;
    emit synchronized(); //stop progress bar

    m_controller = controller;

    if (m_controller) {
        connect(m_controller, &QnAbstractPtzController::finished, this, &QnAbstractPtzDialog::at_controller_finished);
        connect(m_controller, &QnAbstractPtzController::changed,  this, &QnAbstractPtzDialog::at_controller_changed);
        synchronize(tr("Loading..."));
    }
}

void QnAbstractPtzDialog::synchronize(const QString &title) {
    if (!m_controller)
        return;

    if (!isVisible()) {
        emit synchronizeLater(title);
        return;
    }

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability)) {
        m_commands.insert(Qn::GetDataPtzCommand, 0);

        QnPtzData data;
        m_controller->getData(requiredFields(), &data);

        QEventLoop loop;
        connect(this, &QnAbstractPtzDialog::synchronized, &loop, &QEventLoop::quit);

        QList<QWidget*> disabled;
        QLayout* dialogLayout = this->layout();
        QnDialogButtonBox *buttonBox = 0;

        for (int i = 0; i < dialogLayout->count(); i++) {
            QWidget *widget = dialogLayout->itemAt(i)->widget();
            if (!widget)
                continue;

            if (buttonBox || !(buttonBox = qobject_cast<QnDialogButtonBox*>(widget)))
                disabled.append(widget);
        }

        foreach (QWidget* widget, disabled)
            widget->setEnabled(false);

        if (buttonBox) {
            buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, &loop, &QEventLoop::quit);
            buttonBox->showProgress(title);
        }

        loop.exec();

        if (buttonBox) {
            buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            buttonBox->hideProgress();
        }

        foreach (QWidget* widget, disabled)
            widget->setEnabled(true);

    } else {
        m_commands.clear(); // we will not wait for other commands
        m_commands.insert(Qn::GetDataPtzCommand, 0);

        QnPtzData data;
        m_controller->getData(requiredFields(), &data);
        at_controller_finished(Qn::GetDataPtzCommand, QVariant::fromValue(data));
    }
}

bool QnAbstractPtzDialog::activatePreset(const QString &presetId, qreal speed) {
    if (!m_controller)
        return false;

    return m_controller->activatePreset(presetId, speed);
}

bool QnAbstractPtzDialog::createPreset(const QnPtzPreset &preset) {
    if (!m_controller)
        return false;

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability))
        m_commands.insert(Qn::CreatePresetPtzCommand, 0);
    return m_controller->createPreset(preset);
}

bool QnAbstractPtzDialog::updatePreset(const QnPtzPreset &preset) {
    if (!m_controller)
        return false;

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability))
        m_commands.insert(Qn::UpdatePresetPtzCommand, 0);
    return m_controller->updatePreset(preset);
}

bool QnAbstractPtzDialog::removePreset(const QString &presetId) {
    if (!m_controller)
        return false;

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability))
        m_commands.insert(Qn::RemovePresetPtzCommand, 0);
    return m_controller->removePreset(presetId);
}

bool QnAbstractPtzDialog::createTour(const QnPtzTour &tour) {
    if (!m_controller)
        return false;

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability))
        m_commands.insert(Qn::CreateTourPtzCommand, 0);
    return m_controller->createTour(tour);
}

bool QnAbstractPtzDialog::removeTour(const QString &tourId) {
    if (!m_controller)
        return false;

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability))
        m_commands.insert(Qn::RemoveTourPtzCommand, 0);
    return m_controller->removeTour(tourId);
}

bool QnAbstractPtzDialog::updateHomePosition(const QnPtzObject &homePosition) {
    if (!m_controller)
        return false;

    if(m_controller->hasCapabilities(Qn::AsynchronousPtzCapability))
        m_commands.insert(Qn::UpdateHomeObjectPtzCommand, 0);
    return m_controller->updateHomeObject(homePosition);
}

void QnAbstractPtzDialog::at_controller_finished(Qn::PtzCommand command, const QVariant &data) {
    if (m_commands.isEmpty())
        return;

    //TODO: #GDM #PTZ check against validity of data, show error message later if not valid.
    auto pos = m_commands.find(command);
    if (pos == m_commands.end())
        return;
    m_commands.erase(pos);

    if (command == Qn::GetDataPtzCommand){
        if (!m_loaded)
            loadData(data.value<QnPtzData>());
        m_loaded = true;
    }

    if (m_commands.isEmpty()) 
        emit synchronized();
}

void QnAbstractPtzDialog::at_controller_changed(Qn::PtzDataFields fields) {
    updateFields(fields);
}

Qn::PtzCapabilities QnAbstractPtzDialog::capabilities() {
    if (!m_controller)
        return Qn::NoPtzCapabilities;

    return m_controller->getCapabilities();
}

void QnAbstractPtzDialog::saveChanges() {
    saveData();
    synchronize(tr("Saving..."));
}
