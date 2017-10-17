#include "argument_parser.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace utils {

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
                    m_args.emplace(QString::fromUtf8(arg+2, sepPos-(arg+2)), QString::fromUtf8(sepPos+1));
            }
            else
            {
                //short param value
                curParamIter = m_args.emplace(QString::fromUtf8(arg+1), QString());
            }
        }
        else
        {
            //we have value
            if (curParamIter != m_args.end())
                curParamIter->second = QString::fromUtf8(arg);
        }
    }

    NX_LOGX(lm("Parsed: %1").container(m_args), cl_logDEBUG2);
}

bool ArgumentParser::read(const QString& name, QString* const value) const
{
    auto iter = m_args.find(name);
    if (iter == m_args.end())
        return false;

    *value = iter->second;
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

} // namespace utils
} // namespace nx
