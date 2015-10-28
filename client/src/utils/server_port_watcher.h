
#pragma once

#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;

class QnServerPortWatcher : public QObject
{
public:
    QnServerPortWatcher(QnWorkbenchContext *context
        , QObject *parent = nullptr);

    virtual ~QnServerPortWatcher();

private:
    QnMediaServerResourcePtr m_currentServerResource;
};