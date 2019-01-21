#include "commands.h"
#include <assert.h>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <queue>
#include <cstdio>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <grp.h>
#include <nx/system_commands.h>
#include <nx/system_commands/domain_socket/read_linux.h>
#include <nx/system_commands/domain_socket/send_linux.h>

using namespace nx::system_commands::domain_socket;

void registerCommands(CommandsFactory& factory, nx::SystemCommands* systemCommands)
{
    using namespace std::placeholders;

    auto oneArgAction =
        [systemCommands](auto action)
        {
            return
                [systemCommands, action](const std::string& command, int transportFd)
                {
                    std::string commandArg;
                    if (!parseCommand(command, &commandArg))
                        return Result::invalidArg;

                    bool result = action(commandArg);
                    sendInt64(transportFd, result);
                    return result ? Result::ok : Result::execFailed;
                };
        };

    factory.reg({"mkdir"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::makeDirectory, systemCommands, _1)))
    .reg({"rm"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::removePath, systemCommands, _1)))
    .reg(
        {"chown"}, {"path", "uid", "gid", "opt_recursive"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path, uid, gid;
            boost::optional<std::string> isRecursive;

            if (!parseCommand(command, &path, &uid, &gid, &isRecursive))
                return Result::invalidArg;

            bool result = systemCommands->changeOwner(path, std::stoi(uid), std::stoi(gid),
                (bool) isRecursive);
            sendInt64(transportFd, result);
            return result ? Result::ok : Result::execFailed;
        })
    .reg(
        {"mount"}, {"url", "path", "opt_user", "opt_password"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string url, path;
            boost::optional<std::string> user, password;

            if (!parseCommand(command, &url, &path, &user, &password))
                return Result::invalidArg;

            auto result = systemCommands->mount(url, path, user, password);
            sendInt64(transportFd, (int64_t) result);
            return (result == nx::SystemCommands::MountCode::ok) ? Result::ok : Result::execFailed;
        })
    .reg({"mv"}, {"source_path", "target_path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string source, target;
            if (!parseCommand(command, &source, &target))
                return Result::invalidArg;

            bool result = systemCommands->rename(source, target);
            sendInt64(transportFd, (int64_t) result);
            return result ? Result::ok : Result::execFailed;
        })
    .reg({"open"}, {"file_path", "mode"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path, mode;
            if (!parseCommand(command, &path, &mode))
                return Result::invalidArg;

            int resultFd = systemCommands->open(path, std::stoi(mode));
            sendFd(transportFd, resultFd);
            return resultFd > 0 ? Result::ok : Result::execFailed;
        })
    .reg({"freeSpace"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            auto result = systemCommands->freeSpace(path);
            sendInt64(transportFd, result);
            return result > 0 ? Result::ok : Result::execFailed;
        })
    .reg({"totalSpace"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            auto result = systemCommands->totalSpace(path);
            sendInt64(transportFd, result);
            return result > 0 ? Result::ok : Result::execFailed;
        })
    .reg({"exists"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            bool result = systemCommands->isPathExists(path);
            sendInt64(transportFd, (int64_t) result);
            return result ? Result::ok : Result::execFailed;
        })
    .reg({"stat"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            const auto result = systemCommands->stat(path);
            sendBuffer(transportFd, &result, sizeof(nx::SystemCommands::Stats));
            return Result::ok;
        })
    .reg({"list"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            std::string result = systemCommands->serializedFileList(path);
            sendBuffer(transportFd, result.data(), result.size());
            return Result::ok;
        })
    .reg({"devicePath"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            std::string result = systemCommands->devicePath(path);
            sendBuffer(transportFd, result.data(), result.size());
            return !result.empty() ? Result::ok : Result::execFailed;
        })
    .reg({"size"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            int64_t result = systemCommands->fileSize(path);
            sendInt64(transportFd, (int64_t) result);
            return result >= 0 ? Result::ok : Result::execFailed;
        })
    .reg({"dmiInfo"}, {},
        [systemCommands](const std::string& /*command*/, int transportFd)
        {
            std::string result = systemCommands->serializedDmiInfo();
            sendBuffer(transportFd, result.data(), result.size());
            return !result.empty() ? Result::ok : Result::execFailed;
        })
    .reg({"umount", "unmount"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            auto result = systemCommands->unmount(path);
            sendInt64(transportFd, (int64_t) result);
            return result == nx::SystemCommands::UnmountCode::ok ? Result::ok : Result::execFailed;
        })
    .reg({"install"}, {"deb_path", "force"},
         [systemCommands](const std::string& command, int /*transportFd*/)
         {
             std::string debPath;
             boost::optional<std::string> force;
             if (!parseCommand(command, &debPath, &force))
                 return Result::invalidArg;

             std::stringstream commandStream;
             commandStream << "dpkg -i";
             if (force && *force == "force-conflicts")
                commandStream << " -B --force-conflicts";
             commandStream << " '" << debPath << "'";
             int result = ::system(commandStream.str().c_str());
             return result == 0 ? Result::ok : Result::execFailed;
         }, true)
    .reg({"help"}, {},
         [&factory](const std::string& command, int /*transportFd*/)
         {
             std::cout << factory.help() << std::endl;
             return Result::ok;
         }, true);
}

class Worker
{
public:
    using Task = std::function<void()>;

    Worker()
    {
        m_thread = std::thread([this](){ run(); });
    }

    ~Worker()
    {
        stop();
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_needStop = true;
            m_condition.notify_one();
        }

        if (m_thread.joinable())
            m_thread.join();
    }

    void post(const Task& task)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push(task);
        m_condition.notify_one();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

private:
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_needStop = false;
    std::queue<Task> m_tasks;

    void run()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_needStop)
        {
            m_condition.wait(lock, [this]() { return m_needStop || !m_tasks.empty(); });
            if (m_needStop)
                return;

            auto task = m_tasks.front();
            m_tasks.pop();
            lock.unlock();
            task();
            lock.lock();
        }
    }
};

class WorkerPool
{
public:
    explicit WorkerPool(size_t size = 16): m_workers(size)
    {
    }

    void post(const Worker::Task& task)
    {
        auto workerIt = std::min_element(
            m_workers.begin(),
            m_workers.end(),
            [](const Worker& lhs, const Worker& rhs) { return lhs.size() < rhs.size(); });

        assert(workerIt != m_workers.end());
        workerIt->post(task);
    }

private:
    std::vector<Worker> m_workers;
};

class Acceptor
{
public:
    bool bind()
    {
        if ((m_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        {
            perror("socket error");
            return false;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, nx::SystemCommands::kDomainSocket, sizeof(addr.sun_path)-1);

        unlink(nx::SystemCommands::kDomainSocket);
        if (::bind(m_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            perror("bind error");
            return false;
        }

        if (listen(m_fd, 128) == -1)
        {
            perror("listen error");
            return false;
        }

        chmod(nx::SystemCommands::kDomainSocket, 0660);
        #if defined(NX_USER_NAME)
            #define STRINGIFY(v) #v
            #define STRINGIFY2(v) STRINGIFY(v)
            auto groupPtr = getgrnam(STRINGIFY2(NX_USER_NAME));
            if (groupPtr)
            {
                if (chown(nx::SystemCommands::kDomainSocket, geteuid(), groupPtr->gr_gid))
                {
                    perror("chown socket");
                    return false;
                }
            }
            else
            {
                perror("getgrnam failed");
                return false;
            }
            #undef STRINGIFY
            #undef STRINGIFY2
        #endif

        return true;
    }

    int accept()
    {
        return ::accept(m_fd, nullptr, nullptr);
    }

private:
    int m_fd = -1;
};

static void* reallocCallback(void* ctx, ssize_t size)
{
    auto buf = (std::string*) ctx;
    buf->resize(size);
    return buf->data();
}

/**
 * Makes a command-with-args string from the ARGV to make the following processing of commands
 * received via command-line interface uniform with the ones received via socket connection.
 */
static std::string makeCommandString(const char** argv)
{
    std::stringstream commandStream;
    while (*(++argv))
        commandStream << *argv << " ";

    return commandStream.str();
}

/**
 * Command string processing wrapper. COMMAND_STRING might be obtained from 2 sources: the command
 * line or a socket connection.
 */
static int executeCommand(
    const CommandsFactory& commandsFactory,
    const std::string& commandString,
    boost::optional<int> transportSocket)
{
    auto command = commandsFactory.get(commandString, transportSocket);
    if (!command)
    {
        std::fprintf(stdout, "Command %s not found\n", commandString.c_str());
        return -1;
    }

    auto result = execute(command);
    if (!isDirect(command))
        std::fprintf(stdout, "%s --> %s\n", commandString.c_str(), toString(result).c_str());

    switch (result)
    {
        case Result::execFailed:
        case Result::invalidArg:
            return -1;
        case Result::ok:
            return 0;
    }

    return -1;
}

/**
 * This is needed to allow a non-root (Nx) user to execute commands which require super user access
 * such as 'dpkg' as a child process of the root-tool.
 */
static bool setupIds()
{
    if (setreuid(geteuid(), geteuid()) != 0)
    {
        perror("setreuid");
        return false;
    }

    if (setregid(getegid(), getegid()) != 0)
    {
        perror("setregid");
        return false;
    }

    return true;
}

int main(int argc, const char** argv)
{
    fprintf(stdout, "Starting root_tool, using domain socket: %s\n",
        nx::SystemCommands::kDomainSocket);

    nx::SystemCommands systemCommands;
    CommandsFactory commandsFactory;
    registerCommands(commandsFactory, &systemCommands);

    setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc > 1)
    {
        if (!setupIds())
            return -1;
        return executeCommand(commandsFactory, makeCommandString(argv), boost::none);
    }

    WorkerPool workerPool;
    Acceptor acceptor;
    if (!acceptor.bind())
        return -1;

    using namespace nx::system_commands::domain_socket;
    while (true)
    {
        int transportFd = acceptor.accept();
        if (transportFd == -1)
        {
            perror("accept");
            continue;
        }

        workerPool.post(
            [transportFd, &commandsFactory]()
            {
                std::string commandBuf;
                if (!readBuffer(transportFd, &reallocCallback, &commandBuf))
                    return;

                executeCommand(commandsFactory, commandBuf, transportFd);
            });
    }

    return 0;
}
