#pragma once

#include <QtCore/QObject>

#include "translation.h"

class QnTranslationManager: public QObject
{
    Q_OBJECT
public:
    QnTranslationManager(QObject *parent = NULL);
    virtual ~QnTranslationManager();

    const QList<QString>& prefixes() const;
    void setPrefixes(const QList<QString>& prefixes);
    void addPrefix(const QString& prefix);
    void removePrefix(const QString& prefix);

    const QList<QString>& searchPaths() const;
    void setSearchPaths(const QList<QString>& searchPaths);
    void addSearchPath(const QString& searchPath);
    void removeSearchPath(const QString& searchPath);

    QnTranslation defaultTranslation() const;

    QList<QnTranslation> loadTranslations();
    QnTranslation loadTranslation(const QString& locale) const;

    static void installTranslation(const QnTranslation& translation);

    static QString localeCodeToTranslationPath(const QString& localeCode);
    static QString translationPathToLocaleCode(const QString& translationPath);
    static QString localeCode31to30(const QString& localeCode);
    static QString localeCode30to31(const QString& localeCode);

protected:
    QList<QnTranslation> loadTranslationsInternal() const;
    QnTranslation loadTranslationInternal(const QString& translationDir,
        const QString& translationName) const;

private:
    QList<QString> m_searchPaths;
    QList<QString> m_prefixes;
    QList<QnTranslation> m_translations;
    bool m_translationsValid;
};
