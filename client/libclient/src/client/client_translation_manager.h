#ifndef QN_CLIENT_TRANSLATION_MANAGER_H
#define QN_CLIENT_TRANSLATION_MANAGER_H

#include <translation/translation_manager.h>

class QnClientTranslationManager: public QnTranslationManager {
    Q_OBJECT
    typedef QnTranslationManager base_type;

public:
    QnClientTranslationManager(QObject *parent = NULL);
    virtual ~QnClientTranslationManager();

    /**
     * Get locale code of the current translation language.
     * If no corresponding translation found, empty string is returned.
     */
    //TODO: #GDM #Common change method loadTranslations() to be translations() const;
    QString getCurrentLanguage();
};

#endif // QN_CLIENT_TRANSLATION_MANAGER_H
