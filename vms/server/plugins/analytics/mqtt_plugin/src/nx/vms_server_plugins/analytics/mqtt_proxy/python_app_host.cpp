// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "python_app_host.h"

#include <chrono>
#include <iostream>

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include "mqtt_proxy_ini.h"

#if defined(__unix__)
    #include <dlfcn.h>
#endif

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

void CommandQueue::put(const Command& command)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(command);
    }
    m_condition.notify_one();
}

Command CommandQueue::get()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait_for(lock, std::chrono::seconds(5), [this]() { return !m_queue.empty(); });
    if (m_queue.empty())
        return Command{kEmptyRequestId, "", {}};
    Command command = m_queue.front();
    m_queue.pop();
    return command;
}

PYBIND11_EMBEDDED_MODULE(nx_integration_interface, m) {
    pybind11::class_<Command>(m, "Command")
        .def_readonly("request_id", &Command::requestId)
        .def_readonly("command", &Command::command)
        .def_readonly("parameters", &Command::parameters);

    m.attr("IRRELEVANT_REQUEST_ID") = pybind11::int_(kIrrelevantRequestId);
    m.attr("EMPTY_REQUEST_ID") = pybind11::int_(kEmptyRequestId);

    m.def("get_host_request",
        []()
        {
            PythonAppHost& appHost = PythonAppHost::getInstance();
            pybind11::gil_scoped_release release;
            return appHost.getNextRequest();
        });

    m.def("return_app_reply",
        [](int requestId, const std::string reply)
        {
            if (requestId == kIrrelevantRequestId)
                return;
            PythonAppHost& appHost = PythonAppHost::getInstance();
            return appHost.putReply(requestId, reply);
        });
}

void PythonAppHost::callPythonFunction(
    const std::string& command, const std::map<std::string, std::string>& parameters)
{
    Command cmd{kIrrelevantRequestId, command, parameters};
    m_commandQueue.put(cmd);
}

int PythonAppHost::getRequestId()
{
    static int requestId = 0;
    std::lock_guard<std::mutex> lock(m_rvMutex);
    return ++requestId;
}

std::optional<std::string> PythonAppHost::callPythonFunctionWithRv(
    const std::string& command, const std::map<std::string, std::string>& parameters)
{
    const int requestId = getRequestId();

    PythonAppReturnValuePromise promise;
    std::future<std::string> future = promise.get_future();
    {
        std::lock_guard<std::mutex> lock(m_rvMutex);
        m_returnValues[requestId] = std::move(promise);
    }

    Command cmd{requestId, command, parameters};
    m_commandQueue.put(cmd);
    int timeout = ini().appReplyTimeotS;
    std::future_status replyStatus = future.wait_for(std::chrono::seconds(timeout));
    {
        std::lock_guard<std::mutex> lock(m_rvMutex);
        m_returnValues.erase(requestId);
    }

    if (replyStatus != std::future_status::ready)
        return std::nullopt;

    return future.get();
}

void PythonAppHost::putReply(const int requestId, const std::string reply)
{
    std::lock_guard<std::mutex> lock(m_rvMutex);
    auto it = m_returnValues.find(requestId);
    if (it == m_returnValues.end())
    {
        // Request was already processed, or timed out, or never existed (error in python app).
        NX_PRINT << "WARNING: Received reply for unknown request id: " << requestId;
        return;
    }
    it->second.set_value(reply);
}

bool PythonAppHost::preparePythonEnvironment()
{
    // Not needed for Windows platform.
    #if defined(__unix__)
        Dl_info info;
        if (!dladdr((void*) PyConfig_InitPythonConfig, &info))
        {
            NX_PRINT << "ERROR: Failed to find PyConfig_InitPythonConfig addr.";
            return false;
        }

        const std::string pythonDllDirectory = std::string(
            info.dli_fname,
            0,
            strlen(info.dli_fname) - nx::kit::utils::baseName(info.dli_fname).length());

        // Set the environment variables to the directory where the embedded Python resides.
        setenv("PYTHONHOME", pythonDllDirectory.c_str(), 1);
        const std::string pythonPath = pythonDllDirectory + ":"
            + pythonDllDirectory + "/lib" + ":"
            + pythonDllDirectory + "/lib/python3.12" + ":"
            + pythonDllDirectory + "/lib/python3.12/site-packages";
        setenv("PYTHONPATH", pythonPath.c_str(), 1);
    #endif

    return true;
}

void PythonAppHost::pythonServer()
{
    pybind11::scoped_interpreter guard{};
    try
    {
        pybind11::module_ pythonApp = pybind11::module_::import("app");
        pythonApp.attr("main")();
    }
    catch (const pybind11::error_already_set& e)
    {
        NX_PRINT << "ERROR: Import failed while loading MQTT Plugin: " << e.what();
    }
}

} // namespace nx::vms_server_plugins::analytics::mqtt_proxy
