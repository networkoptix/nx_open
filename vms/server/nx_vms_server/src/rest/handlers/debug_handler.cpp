#include "debug_handler.h"

#include <cstdlib>

#include <nx/utils/switch.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/network/http/http_types.h>
#include <mediaserver_ini.h>

namespace {

enum class Action { invalid, crash, exit, delayS };

static std::pair<Action, QString> actionFromParams(const QnRequestParamList& params)
{
    Action action = Action::invalid;
    QString value;
    for (const auto& [stringAction, paramValue]: params)
    {
        const Action paramAction = nx::utils::switch_(stringAction,
            "crash", [](){ return Action::crash; },
            "exit", [](){ return Action::exit; },
            "delayS", [](){ return Action::delayS; },
            nx::utils::default_, [](){ return Action::invalid; });

        if (paramAction != Action::invalid)
        {
            if (action != Action::invalid) //< More than one action param is specified.
                return {Action::invalid, ""};

            action = paramAction;
            value = paramValue;
        }
    }
    return {action, value};
}

struct CrashActionOptions
{
    const bool useFullDump;
};

static CrashActionOptions crashActionOptionsFromParams(const QnRequestParamList& params)
{
    return CrashActionOptions{params.contains("fullDump")};
}

} // namespace

int QnDebugHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& responseMessageBody,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    namespace StatusCode = nx::network::http::StatusCode;

    const auto result =
        [&responseMessageBody](StatusCode::Value statusCode, const QString& message)
        {
            responseMessageBody = message.toUtf8();
            // NOTE: qWarning() is used to make the message appear both on stderr and in the log.
            qWarning().noquote() << "Received /api/serverControl request:" << message;
            return statusCode;
        };

    ini().reload();
    if (!ini().enableApiDebug)
        return result(StatusCode::forbidden, "Ignoring - not enabled via mediaserver.ini");

    const auto [action, value] = actionFromParams(params);
    switch (action)
    {
        case Action::crash:
            return result(StatusCode::ok, lm("Intentionally crashing the server%1").args(
                (crashActionOptionsFromParams(params).useFullDump ? " with full dump" : "")));

        case Action::exit:
            return result(StatusCode::ok, "Exiting the Server via `exit(64)`");

        case Action::invalid:
            return result(
                StatusCode::forbidden,
                "Ignoring - unsupported params " + containerString(params));

        case Action::delayS:
            {
                const auto delayS = value.toInt();
                if (!delayS)
                    return result(StatusCode::forbidden, "Ignoring - invalid delay " + value);

                std::this_thread::sleep_for(std::chrono::seconds(delayS));
                return result(StatusCode::ok, "Delayed reply");
            }
    }
    const QString message = lm("Unexpected enum value: %1").arg(static_cast<int>(action));
    NX_ASSERT(false, message);
    return result(StatusCode::forbidden, "INTERNAL ERROR: " + message);
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
    const auto [action, value] = actionFromParams(params);
    switch (action)
    {
        case Action::crash:
        {
            const bool useFullDump = crashActionOptionsFromParams(params).useFullDump;
            #if defined(_WIN32)
                win32_exception::setCreateFullCrashDump(useFullDump);
            #elif defined(__linux__)
                linux_exception::setSignalHandlingDisabled(useFullDump);
            #endif

            int* const crashPtr = nullptr;
            *crashPtr = 0; //< Intentional crash.
            return;
        }

        case Action::exit:
            exit(64);

        case Action::invalid:
        case Action::delayS:
            return;
    }
    NX_ASSERT(false, lm("Unexpected enum value: %1").arg(static_cast<int>(action)));
}
