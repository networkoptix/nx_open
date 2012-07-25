#ifndef QN_COMMAND_LINE_PARSER_H
#define QN_COMMAND_LINE_PARSER_H

#include <QtCore/QString>
#include <QtCore/QCoreApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QtCore/QList>

class QTextStream;

class QN_EXPORT QnCommandLineParameter {
public:
    QnCommandLineParameter(int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue = QVariant()):
        m_type(type), m_longName(longName), m_shortName(shortName), m_description(description), m_impliedValue(impliedValue)
    {}

    QnCommandLineParameter(int type, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue = QVariant()):
        m_type(type), m_longName(QLatin1String(longName)), m_shortName(QLatin1String(shortName)), m_description(description), m_impliedValue(impliedValue)
    {}

    QnCommandLineParameter(Type type, const char *name, const char *shortName, const char *description):
        m_type(type), m_name(QLatin1String(name)), m_shortName(QLatin1String(shortName)), m_description(QLatin1String(description))
    {}

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
    void addParameter(int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue = QVariant());
    void addParameter(int type, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue = QVariant());

    void print(QTextStream &stream) const;

    QVariant value(const QString &name, const QVariant &defaultValue = QVariant());
    QVariant value(const char *name, const QVariant &defaultValue = QVariant());

    void clear();

    bool parse(int &argc, char **argv, FILE *errorFile);
    bool parse(int &argc, char **argv, QTextStream *errorStream);

private:
    void addName(int index, const QString &name);

private:
    QList<QnCommandLineParameter> m_parameters;
    QList<QVariant> m_values;
    QHash<QString, int> m_indexByName;
};


#endif // QN_COMMAND_LINE_PARSER_H

