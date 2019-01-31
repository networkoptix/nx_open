#pragma once

#include <QtCore/QObject>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {
namespace mobile {

struct InterClientMessage
{
    Q_GADGET

public:
    enum class Command
    {
        startCamerasMode,
        refresh,
        updateUrl
    };
    Q_ENUM(Command)

    Command command;
    QString parameters;

    InterClientMessage();
    InterClientMessage(Command command, const QString& parameters = QString());

    QString toString() const;
    static InterClientMessage fromString(const QString& string);
};

QN_FUSION_DECLARE_FUNCTIONS(InterClientMessage, (json))
QN_FUSION_DECLARE_FUNCTIONS(InterClientMessage::Command, (lexical))

} // namespace mobile
} // namespace client
} // namespace nx
