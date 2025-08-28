// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <tuple>

#include <pybind11/embed.h>

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

static const int kIrrelevantRequestId = -1;
static const int kEmptyRequestId = -2;

using PythonAppReturnValuePromise = std::promise<std::string>;

struct Command
{
    int requestId = 0;
    std::string command;
    std::map<std::string, std::string> parameters;
};

class CommandQueue
{
public:
    void put(const Command& command);

    /** Blocks while the queue is empty. */
    Command get();

private:
    std::queue<Command> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condition;
};

class PythonAppHost
{
public:
    static PythonAppHost& getInstance()
    {
        static PythonAppHost instance;
        return instance;
    }

    void callPythonFunction(
        const std::string& command, const std::map<std::string, std::string>& parameters);

    std::optional<std::string> callPythonFunctionWithRv(
        const std::string& command, const std::map<std::string, std::string>& parameters);

    Command getNextRequest() { return m_commandQueue.get(); }
    void putReply(const int requestId, const std::string reply)  ;

    static bool preparePythonEnvironment();
    static void pythonServer();

private:
    PythonAppHost() = default;

    int getRequestId();

    CommandQueue m_commandQueue;
    std::map<int, PythonAppReturnValuePromise> m_returnValues;
    std::mutex m_rvMutex;
};

} // namespace nx::vms_server_plugins::analytics::mqtt_proxy
