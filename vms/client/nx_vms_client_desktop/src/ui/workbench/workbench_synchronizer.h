// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WORKBENCH_SYNCHRONIZER_H
#define QN_WORKBENCH_SYNCHRONIZER_H

#include <QtCore/QObject>
#include <core/resource/resource_fwd.h>
#include "workbench_context_aware.h"

class QnWorkbench;
class QnWorkbenchLayout;
class QnWorkbenchContext;
class QnWorkbenchSynchronizer;
class QnResourcePool;

/**
 * This class performs bidirectional synchronization of instances of
 * <tt>QnWorkbench</tt> and <tt>QnResourcePool</tt>.
 */
class QnWorkbenchSynchronizer: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;

public:
    QnWorkbenchSynchronizer(QObject *parent = nullptr);

    virtual ~QnWorkbenchSynchronizer();

public slots:
    void submit();

protected slots:
    void at_workbench_layoutsChanged();

private:
    /** Whether changes should be propagated from workbench to resources. */
    bool m_submit;

    /** Whether changes should be propagated from resources to workbench. */
    bool m_update;
};


#endif // QN_WORKBENCH_SYNCHRONIZER_H
