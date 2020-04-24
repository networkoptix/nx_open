#pragma once

#include "translation.h"
#include "translation_overlay.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtCore/QStack>
#include <QtCore/QString>

#include <nx/utils/thread/mutex.h>

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

    /**
     * Temporary changes the locale that is used in the current thread to translate tr() strings.
     * When this object is destroyed, the original locale is restored.
     *
     * In the case of incorrect locale the current locale remains.
     */
    class LocaleRollback
    {
    public:
        LocaleRollback(QnTranslationManager* manager, const QString& locale);
        ~LocaleRollback();
    private:
        QString m_prevLocale;
        QPointer<QnTranslationManager> m_manager;
    };

    static QString localeCodeToTranslationPath(const QString& localeCode);
    static QString translationPathToLocaleCode(const QString& translationPath);
    static QString localeCode31to30(const QString& localeCode);
    static QString localeCode30to31(const QString& localeCode);

protected:
    QList<QnTranslation> loadTranslationsInternal() const;
    QnTranslation loadTranslationInternal(const QString& translationDir,
        const QString& translationName) const;

private:
    bool setCurrentThreadTranslationLocale(const QString& locale);
    QString getCurrentThreadTranslationLocale() const;
    void uninstallUnusedOverlays();

private:
    QList<QString> m_searchPaths;
    QList<QString> m_prefixes;
    QList<QnTranslation> m_translations;
    bool m_translationsValid;

    mutable nx::Mutex m_mutex;
    QHash<Qt::HANDLE, QString> m_threadLocales;
    QHash<QString, QSharedPointer<nx::vms::translation::TranslationOverlay>> m_overlays;

};
