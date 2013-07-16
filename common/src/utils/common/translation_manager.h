#ifndef QN_TRANSLATION_MANAGER_H
#define QN_TRANSLATION_MANAGER_H

#include <QtCore/QObject>

struct QnTranslation {
    QnTranslation() {}
    
    QnTranslation(const QString &languageName, const QString &localeCode, const QString &translationPath): 
        languageName(languageName), localeCode(localeCode), translationPath(translationPath) 
    {}

    /** Language name. */
    QString languageName;

    /** Locale code, e.g. "zh_CN". */
    QString localeCode;

    /** Path to main .qm file. */
    QString translationPath;
};


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

    void installTranslation(const QnTranslation &translation);
    void installTranslation(const QString &translationPath);

protected:
    QList<QnTranslation> loadTranslationsInternal() const;

private:
    QList<QString> m_searchPaths;
    QList<QString> m_prefixes;
    QList<QnTranslation> m_translations;
    bool m_translationsValid;
};

#endif // QN_TRANSLATION_MANAGER_H
