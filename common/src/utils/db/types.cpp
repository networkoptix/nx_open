#include "types.h"

#include <nx/fusion/model_functions.h>

// Using Qt plugin names here.

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::db, RdbmsDriverType,
    (nx::db::RdbmsDriverType::unknown, "bad_driver_name")
    (nx::db::RdbmsDriverType::sqlite, "QSQLITE")
    (nx::db::RdbmsDriverType::mysql, "QMYSQL")
    (nx::db::RdbmsDriverType::postgresql, "QPSQL")
    (nx::db::RdbmsDriverType::oracle, "QOCI")
)
