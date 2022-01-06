// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>


class QnWorkbenchWebPageHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWorkbenchWebPageHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchWebPageHandler();

private:
    void at_newWebPageAction_triggered();
    void at_editWebPageAction_triggered();
};
