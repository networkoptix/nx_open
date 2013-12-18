#include "lexical_functions.h"

void serialize(const bool &value, QString *target) {
    *target = value ? lit("true") : lit("false");
}

bool deserialize(const QString &value, bool *target) {
    /* Also support "0" & "1" during deserialization. */

    if(value == lit("0") || value == lit("false")) {
        *target = false;
        return true;
    } else if(value == lit("1") || value == lit("true")) {
        *target = true;
        return true;
    } else {
        return false;
    }
}

void serialize(const QUuid &value, QString *target) {
    *target = value.toString();
}

bool deserialize(const QString &value, QUuid *target) {
    /* Also support "0" & "1" during deserialization. */

    QUuid result(value);
    if(result.isNull() && value != lit("00000000-0000-0000-0000-000000000000") && value != lit("{00000000-0000-0000-0000-000000000000}"))
        return false;

    *target = result;
    return true;
}

