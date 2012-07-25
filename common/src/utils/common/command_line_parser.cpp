#include "command_line_parser.h"

#include <QtCore/QMetaType>

#include <utils/common/warnings.h>

void QnCommandLineParser::addParameter(const QnCommandLineParameter &parameter) {
    int index = m_parameters.size();
    m_parameters.push_back(parameter);
    m_values.push_back(QVariant());

    if(!parameter.name().isEmpty())
        m_indexByName[parameter.name()] = index;
    if(!parameter.shortName().isEmpty())
        m_indexByName[parameter.shortName()] = index;
}

void QnCommandLineParser::addParameter(int type, const char *name, const char *shortName, const QString &description, const QVariant &impliedValue) {
    addParameter(QnCommandLineParameter(type, name, shortName, description, impliedValue));
}

QVariant QnCommandLineParser::value(const QString &name, const QVariant &defaultValue) {
    int index = m_indexByName.value(name, -1);
    if(index == -1)
        return defaultValue;

    return m_values[index];
}

QVariant QnCommandLineParser::value(const char *name, const QVariant &defaultValue) {
    return value(QLatin1String(name), defaultValue);
}

void QnCommandLineParser::clear() {
    m_parameters.clear();
    m_values.clear();
    m_indexByName.clear();
}

void QnCommandLineParser::print(QTextStream &stream) const {
    /* The dirty way, for now. */

    int shortNameWidth = 0;
    int longNameWidth = 0;
    foreach(const QnCommandLineParameter &parameter, m_parameters) {
        shortNameWidth = qMax(shortNameWidth, parameter.shortName().size());
        longNameWidth = qMax(longNameWidth, parameter.name().size());
    }
    if(longNameWidth > 0)
        longNameWidth++; /* So that there is a single space between long & short names. */

    foreach(const QnCommandLineParameter &parameter, m_parameters) {
        stream.setFieldAlignment(QTextStream::AlignRight);
        
        stream.setFieldWidth(shortNameWidth);
        stream << parameter.shortName();
        
        stream.setFieldWidth(longNameWidth);
        stream << parameter.name();

        stream.setFieldWidth(0);
        stream << " " << parameter.description();

        stream << endl;
    }
}

bool QnCommandLineParser::parse(int &argc, char **argv, bool checkTypes) {
    int pos = 0, skipped = 0;

    QTextStream err(stderr); 

    while(pos < argc) {
        /* Extract name. */
        QStringList paramInfo = QString(QLatin1String(argv[pos])).split('=');
        QString name = paramInfo[0];

        int index = m_indexByName.value(name, -1);
        if(index == -1) {
            argv[skipped] = argv[pos];

            pos++;
            skipped++;
            continue;
        }

        const QnCommandLineParameter &parameter = m_parameters[index];

        /* Extract value. */
        QVariant value;
        if (paramInfo.size() > 1) {
            value = paramInfo[1];
        } else if(parameter.impliedValue().isValid()) {
            value = parameter.impliedValue();
        } else {
            pos++;
            if(pos >= argc) {
                err << tr("No value provided for the '%1' parameter.").arg(name) << endl;
                return false;
            } else {
                value = QLatin1String(argv[pos]);
            }
        }

        /* Convert to typed value. */
        if(checkTypes) {
            QVariant typedValue = value;
            bool success = typedValue.convert(static_cast<QVariant::Type>(parameter.type()));
            if(!success) {
                err << tr("Invalid value for '%1' parameter - expected %2, provided '%3'.").arg(name).arg(QMetaType::typeName(parameter.type())).arg(value.toString()) << endl;
            } else {
                value = typedValue;
            }
        }

        /* Store value. */
        m_values[index] = value;

        pos++;
    }

    argc = skipped;
    return true;
}
