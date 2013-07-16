#ifndef QN_WORKBENCH_TRANSLATION_MANAGER_H
#define QN_WORKBENCH_TRANSLATION_MANAGER_H

#include <translation/translation_manager.h>

#include "workbench_context_aware.h"

class QnWorkbenchTranslationManager: public QnTranslationManager, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnTranslationManager base_type;

public:
    QnWorkbenchTranslationManager(QObject *parent = NULL);
    virtual ~QnWorkbenchTranslationManager();
};

#endif // QN_WORKBENCH_TRANSLATION_MANAGER_H
