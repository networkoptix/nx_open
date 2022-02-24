// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef ABSTRACT_PTZ_DIALOG_H
#define ABSTRACT_PTZ_DIALOG_H

#include <QtCore/QQueue>

#include <client/client_model_types.h>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_constants.h>
#include <nx/vms/client/core/ptz/types.h>
#include <nx/vms/common/ptz/types_fwd.h>
#include <nx/vms/common/ptz/datafield.h>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

class QnAbstractPtzHotkeyDelegate {
public:
    using PresetIdByHotkey = nx::vms::client::core::ptz::PresetIdByHotkey;

    virtual ~QnAbstractPtzHotkeyDelegate() {}
    virtual PresetIdByHotkey hotkeys() const = 0;
    virtual void updateHotkeys(const PresetIdByHotkey &value) = 0;
};

class QnAbstractPtzDialog : public QnSessionAwareButtonBoxDialog {
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    QnAbstractPtzDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = {});
    virtual ~QnAbstractPtzDialog();

    virtual void accept() override;

    QnPtzControllerPtr controller() const;
    void setController(const QnPtzControllerPtr &controller);

    Q_SLOT void saveChanges();

protected:
    using Command = nx::vms::common::ptz::Command;
    using DataField = nx::vms::common::ptz::DataField;
    using DataFields = nx::vms::common::ptz::DataFields;

    virtual void loadData(const QnPtzData &data) = 0;
    virtual void saveData() = 0;
    virtual DataFields requiredFields() const = 0;
    virtual void updateFields(DataFields fields) = 0;

    bool activatePreset(const QString &presetId, qreal speed);
    bool createPreset(const QnPtzPreset &preset);
    bool updatePreset(const QnPtzPreset &preset);
    bool removePreset(const QString &presetId);

    bool createTour(const QnPtzTour &tour);
    bool removeTour(const QString &tourId);

    bool updateHomePosition(const QnPtzObject &homePosition);

    Ptz::Capabilities capabilities();

signals:
    void synchronizeLater(const QString &title);
    void synchronized();

private slots:
    void synchronize(const QString &title);

    void at_controller_finished(Command command, const QVariant& data);
    void at_controller_changed(DataFields fields);

private:
    QnPtzControllerPtr m_controller;

    bool m_loaded;
    QMultiHash<Command, int> m_commands;
};

#endif // ABSTRACT_PTZ_DIALOG_H
