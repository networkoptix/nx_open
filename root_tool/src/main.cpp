#include "command_factory.h"
#include "command.h"
#include <iostream>
#include <assert.h>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <nx/system_commands.h>
#include <nx/system_commands/domain_socket/read_linux.h>
#include <nx/system_commands/domain_socket/send_linux.h>

static boost::optional<std::string> getOptionalArg(const char**& argv)
{
    if (*argv == nullptr)
        return boost::none;

    std::string value(*argv);
    ++argv;
    return value;
}

void registerCommands(CommandsFactory& factory, nx::SystemCommands* systemCommands)
{
    using namespace std::placeholders;

    auto oneArgAction =
        [systemCommands](auto action)
        {
            return
                [systemCommands, action](const char** argv)
                {
                    auto commandArg = getOptionalArg(argv);
                    if (!commandArg)
                        return Result::invalidArg;

                    return action(*commandArg) ? Result::ok : Result::execFailed;
                };
        };


    factory.reg({"mkdir"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::makeDirectory, systemCommands, _1)));
    factory.reg({"rm"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::removePath, systemCommands, _1)));
    factory.reg({"install"}, {"deb_package"},
        oneArgAction(std::bind(&nx::SystemCommands::install, systemCommands, _1)));

    factory.reg(
        {"chown"}, {"path", "opt_recursive"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            boost::optional<std::string> isRecursive;

            if (!parseCommand(&path, &isRecursive))
                return Result::invalidArg;

            auto result = systemCommands->changeOwner(path, (bool) isRecursive);
            nx::system_commands::domain_socket::sendInt64(transportFd, result);
            return result ? Result::ok : Result::execFailed;
        });

    factory.reg(
        {"mount"}, {"url", "path", "opt_user", "opt_password"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto url = getOptionalArg(argv);
            if (!url)
                return Result::invalidArg;

            const auto directory = getOptionalArg(argv);
            if (!directory)
                return Result::invalidArg;

            const auto user = getOptionalArg(argv);
            const auto password = getOptionalArg(argv);

            return systemCommands->mount(
                *url, *directory, user, password, /*usePipe*/ true, std::stol(*socketPostfix))
                    == nx::SystemCommands::MountCode::ok ? Result::ok : Result::execFailed;
        });

    factory.reg({"mv"}, {"source_path", "target_path"},
        [systemCommands](const char** argv)
        {
            const auto source = getOptionalArg(argv);
            if (!source)
                return Result::invalidArg;

            const auto target = getOptionalArg(argv);
            if (!target)
                return Result::invalidArg;

            return systemCommands->rename(*source, *target) ? Result::ok : Result::execFailed;
        });

    factory.reg({"open"}, {"file_path", "mode"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            const auto mode = getOptionalArg(argv);
            if (!mode)
                return Result::invalidArg;

            return systemCommands->open(
                *path, std::stoi(*mode), /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                    ? Result::execFailed : Result::ok;
        });

    factory.reg({"freeSpace"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->freeSpace(*path, /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"totalSpace"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->totalSpace(*path, /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"exists"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            systemCommands->isPathExists(*path, /*usePipe*/ true, std::stol(*socketPostfix));

            return Result::ok;
        });

    factory.reg({"list"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            systemCommands->serializedFileList(*path, /*usePipe*/ true, std::stol(*socketPostfix));

            return Result::ok;
        });

    factory.reg({"devicePath"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            systemCommands->devicePath(*path, /*usePipe*/ true, std::stol(*socketPostfix));

            return Result::ok;
        });

    factory.reg({"size"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->fileSize(*path, /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"kill"}, {"pid"},
        [systemCommands](const char** argv)
        {
            const auto pid = getOptionalArg(argv);
            if (!pid)
                return Result::invalidArg;

            return systemCommands->kill(std::stoi(*pid)) ? Result::ok : Result::execFailed;
        });

    factory.reg({"dmiInfo"}, {},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            return systemCommands->serializedDmiInfo(true, std::stol(*socketPostfix)).empty()
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"umount", "unmount"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->unmount(*path, /*usePipe*/ true, std::stol(*socketPostfix)) ==
                nx::SystemCommands::UnmountCode::ok ? Result::ok : Result::execFailed;
        });

    factory.reg({"help"}, {},
        [&factory](const char** /*argv*/)
        {
            std::cout << factory.help() << std::endl;
            return Result::ok;
        });

    factory.reg({"ids"}, {},
        [systemCommands](const char** /*argv*/)
        {
            systemCommands->showIds();
            return Result::ok;
        });
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
        while (m_needStop)
        {
            m_condition.wait(lock, [this]() { return m_needStop || !m_tasks.empty(); });
            if (m_needStop)
                return;

            auto task = m_tasks.front();
            lock.unlock();
            task();
        }
    }
};

class WorkerPool
{
public:
    WorkerPool(size_t size = 32): m_workers(size)
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

        return true;
    }

    int accept()
    {
        return ::accept(m_fd, nullptr, nullptr);
    }

private:
    int m_fd;
};

static void* reallocCallback(void* ctx, ssize_t size)
{
    std::string* buf = (std::string*) ctx;
    buf->resize(size);
    return buf->data();
}

int main(int /*argc*/, const char** /*argv*/)
{
    nx::SystemCommands systemCommands;
    if (!systemCommands.setupIds())
    {
        std::cout << systemCommands.lastError() << std::endl;
        return -1;
    }

    CommandsFactory commandsFactory;
    registerCommands(commandsFactory, &systemCommands);

    WorkerPool workerPool;
    Acceptor acceptor;
    if (!acceptor.bind())
        return -1;

    using namespace nx::system_commands::domain_socket;
    while (true)
    {
        int clientFd = acceptor.accept();
        if (clientFd == -1)
        {
            perror("accept");
            continue;
        }

        workerPool.post(
            [clientFd, &commandsFactory]()
            {
                std::string buf;
                if (!readBuffer(clientFd, &reallocCallback, &buf))
                    return;

                Command* command = commandsFactory.get(buf);
                if (command == nullptr)
                    return;

                std::cout << "Command {" << buf << "} -> " << command->exec(buf, clientFd);
            });
    }
}
