#ifndef QN_TRANSLATION_MANAGER_H
#define QN_TRANSLATION_MANAGER_H

#include <QtCore/QObject>

#include "translation.h"

class QnTranslationManager: public QObject {
    Q_OBJECT
public:
    QnTranslationManager(QObject *parent = NULL);
    virtual ~QnTranslationManager();

    const QList<QString> &prefixes() const;
    void setPrefixes(const QList<QString> &prefixes);

    const QList<QString> &searchPaths() const;
    void setSearchPaths(const QList<QString> &searchPaths);

    QList<QnTranslation> loadTranslations();
    QnTranslation loadTranslation(const QString &translationPath);

    static void installTranslation(const QnTranslation &translation);

protected:
    QList<QnTranslation> loadTranslationsInternal() const;
    QnTranslation loadTranslationInternal(const QString &translationDir, const QString &translationName) const;

private:
    QList<QString> m_searchPaths;
    QList<QString> m_prefixes;
    QList<QnTranslation> m_translations;
    bool m_translationsValid;
};

#endif // QN_TRANSLATION_MANAGER_H
