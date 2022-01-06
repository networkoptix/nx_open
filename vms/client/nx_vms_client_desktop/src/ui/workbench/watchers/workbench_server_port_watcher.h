// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchServerPortWatcher : public QObject, public QnWorkbenchContextAware
{
    typedef QObject base_type;
public:
    QnWorkbenchServerPortWatcher(QObject *parent = nullptr);

    virtual ~QnWorkbenchServerPortWatcher();

private:
    QnMediaServerResourcePtr m_currentServer;
};
