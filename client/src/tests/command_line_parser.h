#ifndef QN_COMMAND_LINE_PARSER_H
#define QN_COMMAND_LINE_PARSER_H

#include <QString>
#include <QApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <QVariant>
#include <QHash>
#include <QList>

class QnCommandLineParameter {
public:
    enum Type {
        STRING,
        INTEGER,
        FLAG,
    };

    QnCommandLineParameter(Type type, const QString &name, const QString &shortName, const QString &description):
        m_type(type), m_name(name), m_shortName(shortName), m_description(description)
    {}

    QnCommandLineParameter(Type type, const char *name, const char *shortName, const QString &description):
        m_type(type), m_name(QLatin1String(name)), m_shortName(QLatin1String(shortName)), m_description(description)
    {}

    Type type() const {
        return m_type;
    }

    const QString &name() const {
        return m_name;
    }

    const QString &shortName() const {
        return m_shortName;
    }

    const QString &description() const {
        return m_description;
    }

    bool hasValue() const {
        return m_type != FLAG;
    }

private:
    Type m_type;
    QString m_name;
    QString m_shortName;
    QString m_description;
};


class QnCommandLineParser {
    Q_DECLARE_TR_FUNCTIONS(QnCommandLineParser);
public:
    QnCommandLineParser() {}

    void addParameter(const QnCommandLineParameter &parameter);

    QVariant value(const QString &name, const QVariant &defaultValue = QVariant());

    QVariant value(const char *name, const QVariant &defaultValue = QVariant());

    void clear();

    bool parse(int &argc, char **argv);

private:
    QList<QnCommandLineParameter> m_parameters;
    QList<QVariant> m_values;
    QHash<QString, int> m_indexByName;
};


#endif // QN_COMMAND_LINE_PARSER_H

