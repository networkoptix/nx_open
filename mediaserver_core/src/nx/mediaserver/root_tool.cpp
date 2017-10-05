#include "root_tool.h"

#include <QFileInfo>
#include <QDir>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace mediaserver {

RootTool::RootTool(const QString& toolPath):
    m_toolPath(toolPath)
{
    NX_INFO(this, lm("Initialized: %1").arg(toolPath));
}

bool RootTool::mount(const QUrl& url, const QString& path)
{
    QDir().mkdir(path);
    return execute({"mount", "//" + url.host() + url.path(), path, url.userName(), url.password()});
}

bool RootTool::remount(const QUrl& url, const QString& path)
{
    unmount(path);
    return mount(url, path);
}

bool RootTool::unmount(const QString& path)
{
    return execute({"unmount", path});
}

bool RootTool::changeOwner(const QString& path)
{
    return execute({"chown", path});
}

bool RootTool::touchFile(const QString& path)
{
    return execute({"touch", path});
}

bool RootTool::makeDirectory(const QString& path)
{
    return execute({"mkdir", path});
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

bool RootTool::execute(const std::vector<QString>& args)
{
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

    return resultCode == 0;
}

std::unique_ptr<RootTool> findRootTool(const QString& applicationPath)
{
    const auto toolPath = QFileInfo(applicationPath).dir().filePath("root_tool");
    if (QFileInfo(toolPath).exists())
        return std::make_unique<RootTool>(toolPath);

    NX_WARNING(typeid(RootTool), lm("Executable does not exist: %1").arg(toolPath));
    return nullptr;
}

} // namespace mediaserver
} // namespace nx

