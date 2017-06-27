#include "filter.h"

namespace nx {
namespace utils {
namespace db {

QString joinFields(
    const InnerJoinFilterFields& fields,
    const QString& separator)
{
    QString result;
    for (const auto& field : fields)
        result += lit("%1%2=%3")
            .arg(result.isEmpty() ? QString() : separator)
            .arg(QLatin1String(field.name))
            .arg(QLatin1String(field.placeHolderName));
    return result;
}

void bindFields(QSqlQuery* const query, const InnerJoinFilterFields& fields)
{
    for (const auto& field: fields)
        query->bindValue(QLatin1String(field.placeHolderName), field.value);
}

} // namespace db
} // namespace utils
} // namespace nx
