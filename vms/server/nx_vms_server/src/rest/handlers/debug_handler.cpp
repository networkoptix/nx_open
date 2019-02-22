#include "debug_handler.h"

#include <cstdlib>

#include <nx/utils/switch.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/network/http/http_types.h>
#include <mediaserver_ini.h>

namespace {

enum class Action { invalid, crash, exit, newValue };

static Action actionFromParams(const QnRequestParamList& params)
{
    Action action = Action::invalid;
    for (const auto& param: params)
    {
        const Action paramAction = nx::utils::switch_(param.first,
            "crash", [](){ return Action::crash; },
            "exit", [](){ return Action::exit; },
            nx::utils::default_, [](){ return Action::invalid; });

        if (paramAction != Action::invalid)
        {
            if (action != Action::invalid) //< More than one action param is specified.
                return Action::invalid;
            action = paramAction;
        }
    }
    return action;
}

struct CrashActionOptions
{
    const bool useFullDump;
};

static CrashActionOptions crashActionOptionsFromParams(const QnRequestParamList& params)
{
    return CrashActionOptions{params.contains("fullDump")};
}

static void tuneCrashDump(bool useFullDump)
{
    #if defined(_WIN32)
        win32_exception::setCreateFullCrashDump(useFullDump);
    #elif defined(__linux__)
        linux_exception::setSignalHandlingDisabled(useFullDump);
    #endif
}

} // namespace

int QnDebugHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& /*responseMessageBody*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    // NOTE: qWarning() is used to make the messages appear both on stderr and in the log.
    constexpr char messagePrefix[] = "Received /api/serverControl request:";

    ini().reload();
    if (!ini().enableApiServerControl)
    {
        qWarning() << messagePrefix << "Ignoring - not enabled via mediaserver.ini";
        return nx::network::http::StatusCode::forbidden;
    }

// TODO: Remove either branch after NX_ENUM_SWITCH is either approved or removed.
#if 0
    switch (actionFromParams(params))
    {
        case Action::crash:
        {
            const auto options = crashActionOptionsFromParams(params);
            qWarning() << messagePrefix << "Intentionally crashing the server"
                << (options.useFullDump ? "with full crash dump" : "");
            break;
        }

        case Action::exit:
            qWarning() << messagePrefix << "Exiting the Server via `exit(64)`";
            break;

        default:
            qWarning() << messagePrefix << "Ignoring - unsupported params";
            return nx::network::http::StatusCode::forbidden;
    }
    return nx::network::http::StatusCode::ok;
#else // 0
    return NX_ENUM_SWITCH(actionFromParams(params),
    {
        case Action::crash:
        {
            const auto options = crashActionOptionsFromParams(params);
            qWarning() << messagePrefix << "Intentionally crashing the server"
                << (options.useFullDump ? "with full crash dump" : "");
            return nx::network::http::StatusCode::ok;
        }

        case Action::exit:
            qWarning() << messagePrefix << "Exiting the Server via `exit(64)`";
            return nx::network::http::StatusCode::ok;

        case Action::invalid:
            qWarning() << messagePrefix << "Ignoring - unsupported params";
            return nx::network::http::StatusCode::forbidden;
    });
#endif // 0
}

int QnDebugHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*requestBody*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*responseMessageBody*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return nx::network::http::StatusCode::notFound;
}

/**
 * Execution of the action is deferred to afterExecute() to enable returning a message from the
 * request before the Server is stopped.
 */
void QnDebugHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    // Params have to be parsed again in this method, because currently there is no way to pass
    // data from executeGet() to afterExecute() - the instance of this handler is shared between
    // all requests of this API method.

// TODO: Remove either branch after NX_ENUM_SWITCH is either approved or removed.
#if 0
    switch (actionFromParams(params))
    {
        case Action::crash:
        {
            tuneCrashDump(crashActionOptionsFromParams(params).useFullDump);

            int* crashPtr = nullptr;
            *crashPtr = 0; //< Intentional crash.
            break;
        }

        case Action::exit:
            exit(64);

        case Action::invalid:
            return;

        default:
            NX_ASSERT(false);
    }
#else // 0
    NX_ENUM_SWITCH(actionFromParams(params),
    {
        case Action::crash:
        {
            tuneCrashDump(crashActionOptionsFromParams(params).useFullDump);

            int* crashPtr = nullptr;
            *crashPtr = 0; //< Intentional crash.
        }

        case Action::exit:
            exit(64);

        case Action::invalid:
            return;
    });
#endif // 0
}
