#pragma once

#include <nx/fusion/model_functions_fwd.h>

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
    Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(QnServer, ChunksCatalog, )

    enum ChunksCatalog {
        LowQualityCatalog,
        HiQualityCatalog,

        ChunksCatalogCount
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ChunksCatalog)

    enum class StoragePool : int {
        None    = 0,
        Normal  = 1,
        Backup  = 2,
        Both    = Normal | Backup
    };

    inline StoragePool
    operator | (StoragePool lhs, StoragePool rhs) {
        return static_cast<StoragePool>
            (static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    inline StoragePool
    operator & (StoragePool lhs, StoragePool rhs) {
        return static_cast<StoragePool>
            (static_cast<int>(lhs) & static_cast<int>(rhs));
    }
    static const std::string kNoInitStoragesOnStartup = "noInitStoragesOnStartup";
    static const QString kIsConnectedToCloudKey = "isConnectedToCloud";
    static const std::string kNoPlugins = "noPlugins";

} // namespace QnServer
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (QnServer::ChunksCatalog), (metatype)(lexical) )
