#ifndef ABSTRACT_PTZ_DIALOG_H
#define ABSTRACT_PTZ_DIALOG_H

#include <QtCore/QQueue>

#include <common/common_globals.h>

#include <client/client_model_types.h>

#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

class QnAbstractPtzHotkeyDelegate {
public:
    virtual QnPtzHotkeyHash hotkeys() const = 0;
    virtual void updateHotkeys(const QnPtzHotkeyHash &value) = 0;
};

class QnAbstractPtzDialog : public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    QnAbstractPtzDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnAbstractPtzDialog();

    virtual void accept() override;

    QnPtzControllerPtr controller() const;
    void setController(const QnPtzControllerPtr &controller);

    Q_SLOT void saveChanges();

protected:
    virtual void loadData(const QnPtzData &data) = 0;
    virtual void saveData() = 0;
    virtual Qn::PtzDataFields requiredFields() const = 0;
    virtual void updateFields(Qn::PtzDataFields fields) = 0;

    bool activatePreset(const QString &presetId, qreal speed);
    bool createPreset(const QnPtzPreset &preset);
    bool updatePreset(const QnPtzPreset &preset);
    bool removePreset(const QString &presetId);

    bool createTour(const QnPtzTour &tour);
    bool removeTour(const QString &tourId);

    bool updateHomePosition(const QnPtzObject &homePosition);

    Qn::PtzCapabilities capabilities();

signals:
    void synchronizeLater(const QString &title);
    void synchronized();

private slots:
    void synchronize(const QString &title);

    void at_controller_finished(Qn::PtzCommand command, const QVariant &data);
    void at_controller_changed(Qn::PtzDataFields fields);

private:
    QnPtzControllerPtr m_controller;

    bool m_loaded;
    QMultiHash<Qn::PtzCommand, int> m_commands;
};

#endif // ABSTRACT_PTZ_DIALOG_H
