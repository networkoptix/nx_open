#ifndef QN_SERVER_GLOBALS_H
#define QN_SERVER_GLOBALS_H

#include <utils/common/model_functions_fwd.h>

#ifdef Q_MOC_RUN
class QnServer
#else
namespace QnServer
#endif
{
#ifdef Q_MOC_RUN
    Q_GADGET
        Q_ENUMS(ChunksCatalog)
public:
#else
    Q_NAMESPACE
#endif

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

} // namespace QnServer
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (QnServer::ChunksCatalog), (metatype)(lexical) )

#endif // QN_SERVER_GLOBALS_H
