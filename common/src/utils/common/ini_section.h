#ifndef QN_INI_SECTION_H
#define QN_INI_SECTION_H

#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QTextStream>

#include <utils/serialization/lexical_functions.h>

/**
 * Abstraction over a section of an INI file. Much easier to use than <tt>QSettings</tt>,
 * but doesn't support multi-section INI files.
 * 
 * Motivation for creating this class is that this is the format some of the 
 * cameras return data in (e.g. axis, vista).
 */
class QnIniSection {
public:
    QnIniSection() {}

    static QnIniSection fromIni(const QByteArray &ini) {
        QnIniSection result;

        QTextStream stream(ini, QIODevice::ReadOnly);
        while(true) {
            QString line = stream.readLine();
            if(line.isNull())
                break;

            int index = line.indexOf(L'=');
            if(index == -1)
                continue;

            result.insert(line.left(index), line.mid(index + 1));
        }

        return result;
    }

    void insert(const QString &key, const QString &value) {
        m_data[key] = value;
    }

    template<class T>
    T value(const QString &key, const T &defaultValue = T()) const {
        QString result = m_data.value(key);
        if(result.isNull())
            return defaultValue;

        T value;
        if(QnLexical::deserialize(result, &value)) {
            return value;
        } else {
            return defaultValue;
        }
    }

    template<class T>
    bool value(const QString &key, T *value) const {
        QString result = m_data.value(key);
        if(result.isNull())
            return false;

        return QnLexical::deserialize(result, value);
    }

    bool isEmpty() const {
        return m_data.isEmpty();
    }

    void clear() {
        m_data.clear();
    }

private:
#ifdef _DEBUG /* QMap is easier to look through in debug. */
    QMap<QString, QString> m_data;
#else
    QHash<QString, QString> m_data;
#endif
};

#endif // QN_INI_SECTION_H
