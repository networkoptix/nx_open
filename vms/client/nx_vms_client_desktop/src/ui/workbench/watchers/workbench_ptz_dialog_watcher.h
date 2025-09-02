// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;
class QnPtzManageDialog;

class QnWorkbenchPtzDialogWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    QnWorkbenchPtzDialogWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchPtzDialogWatcher() override;

    QnPtzManageDialog* dialog();

    bool isDialogVisible() const;

private:
    void closePtzManageDialog(QnWorkbenchItem* item = nullptr);

private:
    QPointer<QnPtzManageDialog> m_dialog;
};
