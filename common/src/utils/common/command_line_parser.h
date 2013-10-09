#ifndef QN_COMMAND_LINE_PARSER_H
#define QN_COMMAND_LINE_PARSER_H

#include <QtCore/QString>
#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

class QTextStream;

struct QnCommandLineDefaultImpliedValue {};
Q_DECLARE_METATYPE(QnCommandLineDefaultImpliedValue);

namespace detail {
    inline QVariant defaultImpliedValue() {
        return QVariant(qMetaTypeId<QnCommandLineDefaultImpliedValue>(), static_cast<const void *>(NULL));
    }
}

class QN_EXPORT QnCommandLineParameter {
public:
    template<class T>
    QnCommandLineParameter(T *target, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue()) {
        init<T>(target, longName, shortName, description, impliedValue);
    }

    template<class T>
    QnCommandLineParameter(T *target, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue()) {
        init<T>(target, QLatin1String(longName), QLatin1String(shortName), description, impliedValue);
    }

    QnCommandLineParameter(int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue()) {
        init(NULL, type, longName, shortName, description, impliedValue);
    }

    QnCommandLineParameter(int type, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue()) {
        init(NULL, type, QLatin1String(longName), QLatin1String(shortName), description, impliedValue);
    }

    void *target() const {
        return m_target;
    }

    QMetaType *metaType() const {
        return m_metaType.data();
    }

    int type() const {
        return m_type;
    }

    const QString &longName() const {
        return m_longName;
    }

    const QString &shortName() const {
        return m_shortName;
    }

    const QString &description() const {
        return m_description;
    }

    QVariant impliedValue() const {
        return m_impliedValue;
    }

private:
    template<class T>
    void init(T *target, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue) {
        init(target, qMetaTypeId<T>(), longName, shortName, description, impliedValue);
    }

    void init(void *target, int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue);

private:
    void *m_target;
    QSharedPointer<QMetaType> m_metaType;
    int m_type;
    QString m_longName;
    QString m_shortName;
    QString m_description;
    QVariant m_impliedValue;
};


class QN_EXPORT QnCommandLineParser {
    Q_DECLARE_TR_FUNCTIONS(QnCommandLineParser);
public:
    QnCommandLineParser() {}

    void addParameter(const QnCommandLineParameter &parameter);
    void addParameter(int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue());
    void addParameter(int type, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue());

    template<class T>
    void addParameter(T *target, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue = detail::defaultImpliedValue()) {
        addParameter(QnCommandLineParameter(target, longName, shortName, description, impliedValue));
    }

    void print(QTextStream &stream) const;

    QVariant value(const QString &name, const QVariant &defaultValue = QVariant());
    QVariant value(const char *name, const QVariant &defaultValue = QVariant());

    void clear();

    bool parse(const QStringList& args, FILE *errorFile);
    bool parse(const QStringList& args, QTextStream *errorStream);

private:
    void addName(int index, const QString &name);

private:
    QList<QnCommandLineParameter> m_parameters;
    QList<QVariant> m_values;
    QHash<QString, int> m_indexByName;
};


#endif // QN_COMMAND_LINE_PARSER_H

