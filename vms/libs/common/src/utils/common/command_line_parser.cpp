#include "command_line_parser.h"

#include <cassert>

#include <QtCore/QMetaType>
#include <QtCore/QStringList>

#include <utils/common/warnings.h>
#include <nx/utils/log/assert.h>

namespace {

static const QString kUriDelimiter = "://";
static const auto kParamDelimiter = L'=';
};

void QnCommandLineParameter::init(void *target, int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue) {
    m_target = target;
    m_type = type;
    m_metaType.reset(new QMetaType(type));
    m_longName = longName;
    m_shortName = shortName;
    m_description = description;

    if(impliedValue.userType() == qMetaTypeId<QnCommandLineDefaultImpliedValue>()) {
        if(m_type == QMetaType::Bool) {
            m_impliedValue = true; /* Default for booleans. */
        } else {
            m_impliedValue = QVariant();
        }
    } else {
        m_impliedValue = impliedValue;

        if(m_impliedValue.isValid()) {
            if(m_impliedValue.canConvert(static_cast<QVariant::Type>(m_type))) {
                m_impliedValue.convert(static_cast<QVariant::Type>(m_type));
            } else {
                qnWarning("Type of the implied value of command line parameter '%1' does not match parameter's type.", longName);
                m_impliedValue = QVariant();
            }
        }
    }
}


void QnCommandLineParser::addParameter(const QnCommandLineParameter &parameter) {
    int index = m_parameters.size();
    m_parameters.push_back(parameter);
    m_values.push_back(QVariant());

    if(!parameter.longName().isEmpty())
        addName(index, parameter.longName());
    if(!parameter.shortName().isEmpty())
        addName(index, parameter.shortName());
}

void QnCommandLineParser::storeUnparsed(QStringList* unparsed)
{
    m_unparsed = unparsed;
}

void QnCommandLineParser::addName(int index, const QString &name)
{
    int currentIndex = m_indexByName.value(name, -1);
    if(currentIndex != -1 && currentIndex != index) {
        qnWarning("Given parameter name '%1' is already registered with this command line parser. This may lead to unexpected behavior when parsing command line.", name);
        return;
    }

    m_indexByName[name] = index;
}

void QnCommandLineParser::addParameter(int type, const QString &longName, const QString &shortName, const QString &description, const QVariant &impliedValue) {
    addParameter(QnCommandLineParameter(type, longName, shortName, description, impliedValue));
}

void QnCommandLineParser::addParameter(int type, const char *longName, const char *shortName, const QString &description, const QVariant &impliedValue) {
    addParameter(QnCommandLineParameter(type, longName, shortName, description, impliedValue));
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

void QnCommandLineParser::print(QTextStream &stream) const
{
    /* The dirty way, for now. */

    int shortNameWidth = 0;
    int longNameWidth = 0;
    for(const QnCommandLineParameter &parameter: m_parameters) {
        shortNameWidth = qMax(shortNameWidth, parameter.shortName().size());
        longNameWidth = qMax(longNameWidth, parameter.longName().size());
    }
    if(longNameWidth > 0)
        longNameWidth++; /* So that there is a single space between long & short names. */

    for(const QnCommandLineParameter &parameter: m_parameters) {
        stream.setFieldAlignment(QTextStream::AlignRight);

        stream.setFieldWidth(shortNameWidth);
        stream << parameter.shortName();

        stream.setFieldWidth(longNameWidth);
        stream << parameter.longName();

        stream.setFieldWidth(0);
        stream << " " << parameter.description();

        stream << endl;
    }
}

bool QnCommandLineParser::parse(int argc, const char **argv, QTextStream *errorStream)
{
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromUtf8(argv[i]);
    return parse(arguments, errorStream);
}

bool QnCommandLineParser::parse(int argc, const char **argv, FILE *errorFile)
{
    if (errorFile)
    {
        QTextStream errorStream(errorFile);
        return parse(argc, argv, &errorStream);
    }
    else
    {
        return parse(argc, argv, static_cast<QTextStream*>(nullptr));
    }
}

bool QnCommandLineParser::parse(const QStringList& arguments, FILE *errorFile)
{
    if (errorFile)
    {
        QTextStream errorStream(errorFile);
        return parse(arguments, &errorStream);
    }
    else
    {
        return parse(arguments, static_cast<QTextStream*>(nullptr));
    }
}

bool QnCommandLineParser::parse(const QStringList& arguments, QTextStream *errorStream)
{
    bool result = true;
    int pos = 0;

    while (pos < arguments.size())
    {
        /* Extract name. */
        const QString argument = arguments[pos];

        const int delimiterIndex = argument.indexOf(kParamDelimiter);
        const int uriDelimiterIndex = argument.indexOf(kUriDelimiter);
        const bool isSingleUri = uriDelimiterIndex >= 0
            && (delimiterIndex < 0 || uriDelimiterIndex < delimiterIndex);

        const auto paramInfo = isSingleUri
            ? argument.split(kUriDelimiter)
            : argument.split(kParamDelimiter);
        QString name = paramInfo[0];
        if (isSingleUri)
            name += kUriDelimiter;   /* So the registering code looks much better. */

        int index = m_indexByName.value(name, -1);
        if (index == -1)
        {
            if (m_unparsed)
                m_unparsed->append(argument);
            pos++;
            continue;
        }

        const QnCommandLineParameter &parameter = m_parameters[index];

        /* Extract value. */
        QVariant value;
        if (paramInfo.size() > 1)
        {
            value = paramInfo[1];
        }
        else if (parameter.impliedValue().isValid())
        {
            value = parameter.impliedValue();
        }
        else
        {
            pos++;
            if (pos >= arguments.size())
            {
                if (errorStream)
                {
                    *errorStream
                        << lm("No value provided for the '%1' argument.").arg(name) << endl;
                }
                result = false;
            }
            else
            {
                value = arguments[pos];
            }
        }

        /* Convert to typed value. */
        QVariant typedValue = value;
        bool success = typedValue.convert(static_cast<QVariant::Type>(parameter.type()));
        if (!success)
        {
            if (errorStream)
            {
                *errorStream
                    << lm("Invalid value for '%1' argument - expected %2, provided '%3'.").args(
                        name, QMetaType::typeName(parameter.type()), value.toString())
                    << endl;
            }
            result = false;
        }
        else
        {
            value = typedValue;
        }

        /* Store value. */
        m_values[index] = value;

        /* Write value out if needed. */
        if (parameter.target() && parameter.metaType() && result)
        {
            NX_ASSERT(value.userType() == parameter.type());

            parameter.metaType()->construct(parameter.target(), value.data());
        }

        pos++;
    }

    return result;
}
