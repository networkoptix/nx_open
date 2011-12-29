#include "command_line_parser.h"
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

bool QnCommandLineParser::parse(int &argc, char **argv) {
    int pos = 0, skipped = 0;

    QTextStream err(stderr);

    while(pos < argc) {
        /* Extract name. */
        QString name = QLatin1String(argv[pos]);
        int index = m_indexByName.value(name, -1);
        if(index == -1) {
            argv[skipped] = argv[pos];

            pos++;
            skipped++;
            continue;
        }

        const QnCommandLineParameter &parameter = m_parameters[index];

        /* Extract string value if needed. */
        QString stringValue;
        if(parameter.hasValue()) {
            pos++;
            if(pos > argc) {
                err << tr("No value provided for the '%1' parameter.").arg(name);
                return false;
            } else {
                stringValue = QLatin1String(argv[pos]);
            }
        }

        /* Convert to typed value. */
        QVariant value;
        switch(parameter.type()) {
        case QnCommandLineParameter::STRING:
            value = stringValue;
            break;
        case QnCommandLineParameter::INTEGER: {
            bool ok;
            qint64 intValue = stringValue.toLongLong(&ok);
            if(!ok) {
                err << tr("Invalid value for '%1' parameter - expected integer, provided '%2'.").arg(name).arg(stringValue);
                return false;
            } else {
                value = intValue;
            }
            break;
                                              }
        case QnCommandLineParameter::FLAG:
            value = true;
            break;
        default:
            qnWarning("Invalid command line parameter type '%1'.", static_cast<int>(parameter.type()));
            break;
        }

        /* Store value. */
        m_values[index] = value;

        pos++;
    }

    argc = skipped;
    return true;
}