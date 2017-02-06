#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchServerSafemodeWatcher: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchServerSafemodeWatcher(QObject *parent = nullptr);

private:
    void updateCurrentServer();
    void updateServerFlags();

private:
    QnMediaServerResourcePtr m_currentServer;
};
