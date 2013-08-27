#ifndef QN_CLIENT_TRANSLATION_MANAGER_H
#define QN_CLIENT_TRANSLATION_MANAGER_H

#include <translation/translation_manager.h>

class QnClientTranslationManager: public QnTranslationManager {
    Q_OBJECT
    typedef QnTranslationManager base_type;

public:
    QnClientTranslationManager(QObject *parent = NULL);
    virtual ~QnClientTranslationManager();
};

#endif // QN_CLIENT_TRANSLATION_MANAGER_H
