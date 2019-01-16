#pragma once

#include <rest/helpers/request_context.h>

class QnCommonModule;
class QnEmptyRequestData;

namespace nx::update { struct Status; };

namespace detail {

void checkUpdateStatusRemotely(
    const QList<QnUuid>& participants,
    QnCommonModule* commonModule,
    const QString& path,
    QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context);


} // namespace detail