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
        oneArgAction(std::bind(&nx::SystemCommands::makeDirectory, systemCommands, _1)));
    factory.reg({"rm"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::removePath, systemCommands, _1)));
    factory.reg({"install"}, {"deb_package"},
        oneArgAction(std::bind(&nx::SystemCommands::install, systemCommands, _1)));

    factory.reg(
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
        });

    factory.reg(
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
        });

    factory.reg({"mv"}, {"source_path", "target_path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string source, target;
            if (!parseCommand(command, &source, &target))
                return Result::invalidArg;

            bool result = systemCommands->rename(source, target);
            sendInt64(transportFd, (int64_t) result);
            return result ? Result::ok : Result::execFailed;
        });

    factory.reg({"open"}, {"file_path", "mode"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path, mode;
            if (!parseCommand(command, &path, &mode))
                return Result::invalidArg;

            int resultFd = systemCommands->open(path, std::stoi(mode));
            sendFd(transportFd, resultFd);
            return resultFd > 0 ? Result::ok : Result::execFailed;
        });

    factory.reg({"freeSpace"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            auto result = systemCommands->freeSpace(path);
            sendInt64(transportFd, result);
            return result > 0 ? Result::ok : Result::execFailed;
        });

    factory.reg({"totalSpace"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            auto result = systemCommands->totalSpace(path);
            sendInt64(transportFd, result);
            return result > 0 ? Result::ok : Result::execFailed;
        });

    factory.reg({"exists"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            bool result = systemCommands->isPathExists(path);
            sendInt64(transportFd, (int64_t) result);
            return result ? Result::ok : Result::execFailed;
        });

    factory.reg({"list"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            std::string result = systemCommands->serializedFileList(path);
            sendBuffer(transportFd, result.data(), result.size());
            return Result::ok;
        });

    factory.reg({"devicePath"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            std::string result = systemCommands->devicePath(path);
            sendBuffer(transportFd, result.data(), result.size());
            return !result.empty() ? Result::ok : Result::execFailed;
        });

    factory.reg({"size"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            int64_t result = systemCommands->fileSize(path);
            sendInt64(transportFd, (int64_t) result);
            return result >= 0 ? Result::ok : Result::execFailed;
        });

    factory.reg({"dmiInfo"}, {},
        [systemCommands](const std::string& /*command*/, int transportFd)
        {
            std::string result = systemCommands->serializedDmiInfo();
            sendBuffer(transportFd, result.data(), result.size());
            return !result.empty() ? Result::ok : Result::execFailed;
        });

    factory.reg({"umount", "unmount"}, {"path"},
        [systemCommands](const std::string& command, int transportFd)
        {
            std::string path;
            if (!parseCommand(command, &path))
                return Result::invalidArg;

            auto result = systemCommands->unmount(path);
            sendInt64(transportFd, (int64_t) result);
            return result == nx::SystemCommands::UnmountCode::ok ? Result::ok : Result::execFailed;
        });

    factory.reg({"help"}, {},
        [&factory](const std::string& /*command*/, int /*transportFd*/)
        {
            std::cout << factory.help() << std::endl;
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
    WorkerPool(size_t size = 16): m_workers(size)
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

        chmod(nx::SystemCommands::kDomainSocket, 0777);

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
                {
                    std::cout << "Command " << buf << " not found" << std::endl;
                    return;
                }

                command->exec(buf, clientFd);
            });
    }
}
