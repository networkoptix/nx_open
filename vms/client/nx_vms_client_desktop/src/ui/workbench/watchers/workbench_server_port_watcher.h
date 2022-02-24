// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchServerPortWatcher : public Connective<QObject>, public QnWorkbenchContextAware
{
    typedef Connective<QObject> base_type;
public:
    QnWorkbenchServerPortWatcher(QObject *parent = nullptr);

    virtual ~QnWorkbenchServerPortWatcher();

private:
    QnMediaServerResourcePtr m_currentServer;
};