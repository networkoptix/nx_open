#include "business_event_parameters.h"

#include <QtCore/QVariant>

#include "events/abstract_business_event.h"

#include <utils/common/id.h>
#include <utils/common/model_functions.h>

QnBusinessEventParameters::QnBusinessEventParameters():
    eventType(QnBusiness::UndefinedEvent),
    eventTimestampUsec(0),
    reasonCode(QnBusiness::NoReason)
{}

QnUuid QnBusinessEventParameters::getParamsHash() const {
    QByteArray paramKey(QByteArray::number(eventType));
    switch (eventType) {
        case QnBusiness::ServerFailureEvent:
        case QnBusiness::StorageFailureEvent:
            paramKey += '_' + QByteArray::number(reasonCode);
            if (reasonCode == QnBusiness::StorageIoErrorReason 
                || reasonCode == QnBusiness::StorageTooSlowReason 
                || reasonCode == QnBusiness::StorageFullReason 
                || reasonCode == QnBusiness::LicenseRemoved)
                paramKey += '_' + description.toUtf8();
            break;

        case QnBusiness::CameraInputEvent:
            paramKey += '_' + inputPortId.toUtf8();
            break;

        case QnBusiness::CameraDisconnectEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            break;

        case QnBusiness::NetworkIssueEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            paramKey += '_' + QByteArray::number(reasonCode);
            break;

        default:
            break;
    }
    return guidFromArbitraryData(paramKey);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnEventMetaData, (ubjson)(json)(eq)(xml)(csv_record), QnEventMetaData_Fields, (optional, true) )
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBusinessEventParameters, (ubjson)(json)(eq)(xml)(csv_record), QnBusinessEventParameters_Fields, (optional, true) )
