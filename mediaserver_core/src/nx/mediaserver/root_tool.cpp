
#include <QFileInfo>
#include <QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#if defined(Q_OS_LINUX)
    #include <nx/root_tool/actions.h>
#endif
#include "root_tool.h"

namespace nx {
namespace mediaserver {

RootTool::RootTool(const QString& toolPath):
    m_toolPath(toolPath)
{
    NX_INFO(this, lm("Initialized: %1").arg(toolPath));
}

Qn::StorageInitResult RootTool::mount(const QUrl& url, const QString& path)
{
#if defined (Q_OS_LINUX)
    makeDirectory(path);

    auto uncString = "//" + url.host() + url.path();
    if (m_toolPath.isEmpty())
    {
        root_tool::Actions actions;
        if (!actions.mount(
                uncString.toStdString(),
                path.toStdString(),
                url.userName().isEmpty() ? boost::none : boost::make_optional(url.userName().toStdString()),
                url.password().isEmpty() ? boost::none : boost::make_optional(url.password().toStdString())))
        {
            NX_WARNING(this, lm("[mount] Mount via root_tool::Actions failed. url: %1, path: %2")
                .args(url.toString(), path));

            auto output = actions.lastError();
            if (output.find("13") != std::string::npos)
                return Qn::StorageInit_WrongAuth;

            return Qn::StorageInit_WrongPath;
        }

        NX_VERBOSE(this, lm("[mount] Mount via root_tool::Actions successful. url: %1, path: %2")
            .args(url.toString(), path));

        return Qn::StorageInit_Ok;
    }

    std::vector<QString> args = {"mount", uncString, path};

    if (!url.userName().isEmpty())
        args.push_back(url.userName());

    if (!url.password().isEmpty())
        args.push_back(url.password());

    int retCode = execute(args);
    if (retCode != 0)
    {
        NX_WARNING(this, lm("[mount] Mount via root_tool application failed. url: %1, path: %2, retCode: %3")
            .args(url.toString(), path, retCode));

        if (retCode == 13)
            return Qn::StorageInit_WrongAuth;
        return Qn::StorageInit_WrongPath;
    }

    NX_VERBOSE(this, lm("[mount] Mount via root_tool application successful. url: %1, path: %2")
        .args(url.toString(), path));

    return Qn::StorageInit_Ok;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return Qn::StorageInit_WrongPath;
#endif
}

Qn::StorageInitResult RootTool::remount(const QUrl& url, const QString& path)
{
#if defined (Q_OS_LINUX)
    bool result = unmount(path);
    NX_VERBOSE(
        this,
        lm("[initOrUpdate, mount] root_tool unmount %1 result %2").args(path, result));

    auto mountResult = mount(url, path);
    NX_VERBOSE(
        this,
        lm("[initOrUpdate, mount] root_tool mount %1 to %2 result %3").args(url, path, result));

    return mountResult;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return Qn::StorageInit_WrongPath;
#endif
}

bool RootTool::unmount(const QString& path)
{
#if defined (Q_OS_LINUX)
    if (m_toolPath.isEmpty())
    {
        root_tool::Actions actions;
        if (!actions.unmount(path.toStdString()))
            return false;

        return true;
    }

    return execute({"unmount", path}) == 0;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return false;
#endif
}

bool RootTool::changeOwner(const QString& path)
{
#if defined (Q_OS_LINUX)
    if (m_toolPath.isEmpty())
    {
        root_tool::Actions actions;
        if (!actions.changeOwner(path.toStdString()))
            return false;

        return true;
    }

    return execute({"chown", path}) == 0;
#else
    return false;
#endif
}

bool RootTool::touchFile(const QString& path)
{
#if defined (Q_OS_LINUX)
    if (m_toolPath.isEmpty())
    {
        root_tool::Actions actions;
        if (!actions.touchFile(path.toStdString()))
            return false;

        return true;
    }

    return execute({"touch", path}) == 0;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return false;
#endif
}

bool RootTool::makeDirectory(const QString& path)
{
#if defined (Q_OS_LINUX)
    if (m_toolPath.isEmpty())
    {
        root_tool::Actions actions;
        if (!actions.makeDirectory(path.toStdString()))
            return false;

        return true;
    }

    return execute({"mkdir", path}) == 0;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return false;
#endif
}

static std::string makeArgsLine(const std::vector<QString>& args)
{
    std::ostringstream argsStream;
    for (const auto& arg: args)
        argsStream << "'" << arg.toStdString() << "' ";

    return argsStream.str();
}

static std::string makeCommand(const QString& toolPath, const std::string& argsLine)
{
    std::ostringstream runStream;
    runStream << "'" << toolPath.toStdString() << "' " << argsLine << "2>&1";
    return runStream.str();
}

int  RootTool::execute(const std::vector<QString>& args)
{
#if defined( Q_OS_LINUX )
    const auto argsLine = makeArgsLine(args);
    const auto commandLine = makeCommand(m_toolPath, argsLine);
    const auto pipe = popen(commandLine.c_str(), "r");
    if (pipe == nullptr)
    {
        NX_DEBUG(this, lm("Popen %1 has failed: %2").args(
            m_toolPath, SystemError::getLastOSErrorText()));

        return false;
    }

    std::ostringstream outputStream;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        outputStream << buffer;

    const auto resultCode = pclose(pipe);
    NX_DEBUG(this, lm("%1 -- %2").args(argsLine, resultCode));

    auto output = outputStream.str();
    output.erase(output.find_last_not_of(" \n\r\t") + 1);
    if (output.size() != 0)
        NX_VERBOSE(this, output);

    return resultCode;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return false;
#endif
}

std::unique_ptr<RootTool> findRootTool(const QString& applicationPath)
{
    const auto toolPath = QFileInfo(applicationPath).dir().filePath("root_tool");
    bool isRootToolExists = QFileInfo(toolPath).exists();
    if (!isRootToolExists)
        NX_WARNING(typeid(RootTool), lm("Executable does not exist: %1").arg(toolPath));

    return std::make_unique<RootTool>(isRootToolExists ? toolPath : QString());
}

} // namespace mediaserver
} // namespace nx

