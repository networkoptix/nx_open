// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "argument_parser.h"

#include <nx/utils/log/log.h>

namespace nx {

ArgumentParser::ArgumentParser(int argc, const char* argv[])
{
    if (argc && argv)
        parse(argc, argv);
}

ArgumentParser::ArgumentParser(const QStringList& args)
{
    parse(args);
}

void ArgumentParser::parse(const QStringList& args)
{
    std::vector<QByteArray> latin1Args;
    for (const auto& arg: args)
        latin1Args.push_back(arg.toLocal8Bit());
    std::vector<const char*> argv;
    for (const auto& arg: latin1Args)
        argv.push_back(arg.data());
    parse((int) latin1Args.size(), &argv[0]);
}

void ArgumentParser::parse(int argc, const char* argv[])
{
    std::multimap<QString, QString>::iterator curParamIter = m_args.end();

    for (int i = 0; i < argc; ++i)
    {
        const char* arg = argv[i];

        const auto argLen = strlen(arg);
        if (argLen == 0)
            continue;

        if (arg[0] == '-')
        {
            //param
            if (argLen > 1 && arg[1] == '-')    //long param
            {
                //parsing long param
                const char* sepPos = strchr(arg, '=');
                if (sepPos == nullptr)
                    m_args.emplace(QString::fromUtf8(arg+2), QString()); //no value
                else
                    m_args.emplace(QString::fromUtf8(arg+2, (int) (sepPos-(arg+2))), QString::fromUtf8(sepPos+1));
            }
            else
            {
                //short param value
                curParamIter = m_args.emplace(QString::fromUtf8(arg+1), QString());
            }
        }
        else
        {
            if (curParamIter != m_args.end())
            {
                // We have named value.
                curParamIter->second = QString::fromUtf8(arg);
                curParamIter = m_args.end();
            }
            else
            {
                // We have unnamed value.
                m_positionalArgs.push_back(QString::fromUtf8(arg));
            }
        }
    }
}

bool ArgumentParser::read(const QString& name, QString* const value) const
{
    auto iter = m_args.find(name);
    if (iter == m_args.end())
        return false;

    *value = iter->second;
    return true;
}

bool ArgumentParser::read(const QString& name, std::string* const value) const
{
    QString strValue;
    if (!read(name, &strValue))
        return false;

    *value = strValue.toStdString();
    return true;
}

bool ArgumentParser::read(const QString& name, int* const value) const
{
    QString strValue;
    if (!read(name, &strValue))
        return false;

    *value = strValue.toInt();
    return true;
}

bool ArgumentParser::read(const QString& name, size_t* const value) const
{
    QString strValue;
    if (!read(name, &strValue))
        return false;

    *value = strValue.toUInt();
    return true;
}

bool ArgumentParser::read(const QString& name, double* const value) const
{
    QString strValue;
    if (!read(name, &strValue))
        return false;

    *value = strValue.toDouble();
    return true;
}

bool ArgumentParser::contains(const QString& name) const
{
    return m_args.count(name) > 0;
}

std::multimap<QString, QString> ArgumentParser::allArgs() const
{
    return m_args;
}

std::vector<QString> ArgumentParser::getPositionalArgs() const
{
    return m_positionalArgs;
}

} // namespace nx
