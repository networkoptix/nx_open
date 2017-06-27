#include "event_parameters.h"

#include <QtCore/QVariant>

#include <nx/fusion/model_functions.h>
#include <utils/common/id.h>

namespace nx {
namespace vms {
namespace event {

EventParameters::EventParameters():
    eventType(UndefinedEvent),
    eventTimestampUsec(0),
    reasonCode(NoReason)
{
}

QnUuid EventParameters::getParamsHash() const
{
    QByteArray paramKey(QByteArray::number(eventType));
    switch (eventType)
    {
        case ServerFailureEvent:
        case StorageFailureEvent:
            paramKey += '_' + QByteArray::number(reasonCode);
            if (reasonCode == StorageIoErrorReason
                || reasonCode == StorageTooSlowReason
                || reasonCode == StorageFullReason
                || reasonCode == SystemStorageFullReason
                || reasonCode == LicenseRemoved)
            {
                paramKey += '_' + description.toUtf8();
            }
            break;

        case SoftwareTriggerEvent:
            return QnUuid::createUuid(); //< Warning: early return.
            break;

        case CameraInputEvent:
            paramKey += '_' + inputPortId.toUtf8();
            break;

        case CameraDisconnectEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            break;

        case NetworkIssueEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            paramKey += '_' + QByteArray::number(reasonCode);
            break;

        default:
            break;
    }

    return guidFromArbitraryData(paramKey);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (EventMetaData)(EventParameters),
    (ubjson)(json)(eq)(xml)(csv_record), _Fields, (brief, true))

} // namespace event
} // namespace vms
} // namespace nx
