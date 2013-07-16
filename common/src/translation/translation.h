#ifndef QN_TRANSLATION_H
#define QN_TRANSLATION_H

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

class QnTranslationPrivate;

class QnTranslation {
public:
    QnTranslation() {}
    
    QnTranslation(const QString &languageName, const QString &localeCode, const QString &suffix, const QStringList &filePaths):
        m_languageName(languageName), 
        m_localeCode(localeCode), 
        m_suffix(suffix), 
        m_filePaths(filePaths) 
    {}

    QString languageName() const {
        return m_languageName;
    }

    QString localeCode() const {
        return m_localeCode;
    }

    QString suffix() const {
        return m_suffix;
    }

    QStringList filePaths() const {
        return m_filePaths;
    }

    bool isEmpty() const {
        return m_filePaths.isEmpty();
    }

private:
    /** Language name. */
    QString m_languageName;

    /** Locale code, e.g. "zh_CN". */
    QString m_localeCode;

    /** Suffix of .qm files constituting a translation. */
    QString m_suffix;

    /** Paths to .qm files. */
    QStringList m_filePaths;
};
Q_DECLARE_METATYPE(QnTranslation)


#endif // QN_TRANSLATION_H


