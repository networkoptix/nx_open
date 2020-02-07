#pragma once

#include "translation.h"
#include "translation_overlay.h"

#include <QtCore/QObject>
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

    struct LocaleRollback
    {
        ~LocaleRollback() { manager->popThreadTranslationLocale(); }
        QnTranslationManager* manager;
    };
    using ScopedLocaleRollback = QScopedPointer<LocaleRollback>;
    /**
     * Temporary changes the locale that is used in the current thread to translate tr() strings.
     * Returns a temporary object. When this object is destroyed, the original locale is restored.
     *
     * In the case of incorrect locale an empty pointer is returned and the current locale remains.
     * This method is reenterable, so it's possible to create a 'stack' of locales if necessary.
     */
    ScopedLocaleRollback alterThreadLocale(const QString& locale);

    static QString localeCodeToTranslationPath(const QString& localeCode);
    static QString translationPathToLocaleCode(const QString& translationPath);
    static QString localeCode31to30(const QString& localeCode);
    static QString localeCode30to31(const QString& localeCode);

protected:
    QList<QnTranslation> loadTranslationsInternal() const;
    QnTranslation loadTranslationInternal(const QString& translationDir,
        const QString& translationName) const;

private:
    bool pushThreadTranslationLocale(const QString& locale);
    bool popThreadTranslationLocale();

private:
    QList<QString> m_searchPaths;
    QList<QString> m_prefixes;
    QList<QnTranslation> m_translations;
    bool m_translationsValid;

    nx::utils::Mutex m_mutex;
    QHash<Qt::HANDLE, QStack<QString>> m_localeStack;
    QHash<QString, QSharedPointer<nx::vms::translation::TranslationOverlay>> m_overlays;

};
