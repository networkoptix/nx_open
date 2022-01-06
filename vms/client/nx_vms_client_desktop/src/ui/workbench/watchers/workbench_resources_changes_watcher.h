// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchResourcesChangesWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchResourcesChangesWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchResourcesChangesWatcher();

private:
    void showWarningDialog(const QnResourceList& resources);
    void showDeleteErrorDialog(const QnResourceList& resources);
};
