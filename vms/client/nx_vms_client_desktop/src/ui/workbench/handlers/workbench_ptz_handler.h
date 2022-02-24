// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPtzHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QnWorkbenchPtzHandler(QObject* parent = nullptr);
    ~QnWorkbenchPtzHandler();

private slots:
    void at_ptzActivatePresetAction_triggered();
    void at_ptzActivateTourAction_triggered();
    void at_ptzActivateObjectAction_triggered();

    void at_ptzSavePresetAction_triggered();
    void at_ptzManageAction_triggered();

    void at_debugCalibratePtzAction_triggered();
    void at_debugGetPtzPositionAction_triggered();

    void at_ptzContinuousMoveAction_triggered();
    void at_ptzActivatePresetByIndexAction_triggered();
    void at_ptzActivateObjectByHotkeyAction_triggered();
    void at_ptzFocusInAction_triggered();
    void at_ptzFocusOutAction_triggered();
    void at_ptzFocusAutoAction_triggered();

    void ptzFocusMove(double speed);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
