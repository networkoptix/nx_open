#include "api_media_server_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {

    backup::DayOfWeek backup::fromQtDOW( Qt::DayOfWeek qtDOW ) {
        switch (qtDOW)
        {
        case Qt::Monday:
            return backup::Monday;
        case Qt::Tuesday:
            return backup::Tuesday;
        case Qt::Wednesday:
            return backup::Wednesday;
        case Qt::Thursday:
            return backup::Thursday;
        case Qt::Friday:
            return backup::Friday;
        case Qt::Saturday:
            return backup::Saturday;
        case Qt::Sunday:
            return backup::Sunday;
        default:
            return backup::Never;
        }
    }

    backup::DaysOfWeek backup::fromQtDOW( QList<Qt::DayOfWeek> days ) {
        backup::DaysOfWeek result;
        for (Qt::DayOfWeek day: days)
            result |= fromQtDOW(day);
        return result;
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiStorageData) (ApiMediaServerData) (ApiMediaServerUserAttributesData) (ApiMediaServerDataEx), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))

        QN_FUSION_DEFINE_FUNCTIONS(ApiMediaServerData, (eq))
        QN_FUSION_DEFINE_FUNCTIONS(ApiMediaServerUserAttributesData, (eq))
} // namespace ec2
