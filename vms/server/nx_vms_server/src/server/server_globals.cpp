#include "server_globals.h"
#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnServer, ChunksCatalog)

QString QnServer::toString(StoragePool pool)
{
    switch (pool)
    {
        case StoragePool::Both: return "Both";
        case StoragePool::None: return "None";
        case StoragePool::Backup: return "Backup";
        case StoragePool::Normal: return "Normal";
    }

    return "";
}
