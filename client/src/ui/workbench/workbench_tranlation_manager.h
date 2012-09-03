#ifndef QN_WORKBENCH_TRANSLATION_MANAGER_H
#define QN_WORKBENCH_TRANSLATION_MANAGER_H

#include <QtCore/QObject>

#include "workbench_context_aware.h"

struct QnTranslationInfo {
    QnTranslationInfo() {}
    
    QnTranslationInfo(const QString &languageName, const QString &localeCode, const QString &translationPath): 
        languageName(languageName), localeCode(localeCode), translationPath(translationPath) 
    {}

    /** Language name. */
    QString languageName;

    /** Locale code, e.g. "zh_CN". */
    QString localeCode;

    /** Path to .qm file. */
    QString translationPath;
};


class QnWorkbenchTranslationManager: public QObject, public QnWorkbenchContextAware {
public:
    QnWorkbenchTranslationManager(QObject *parent = NULL);
    virtual ~QnWorkbenchTranslationManager();

    const QList<QnTranslationInfo> &translations() const;

    static void installTranslation(const QString &translationPath);

protected:
    void ensureTranslations() const;

private:
    mutable bool m_translationsValid;
    mutable QList<QnTranslationInfo> m_translations;
};

#endif // QN_WORKBENCH_TRANSLATION_MANAGER_H
