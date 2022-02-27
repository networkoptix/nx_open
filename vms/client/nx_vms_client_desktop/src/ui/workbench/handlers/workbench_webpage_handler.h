// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchWebPageHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWorkbenchWebPageHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchWebPageHandler();

private:
    void at_newWebPageAction_triggered();
    void at_editWebPageAction_triggered();
};
