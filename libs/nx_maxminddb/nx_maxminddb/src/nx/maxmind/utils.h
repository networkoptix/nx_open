#include "maxminddb.h"

namespace nx::maxmind::utils {

const char* dataTypeToString(int mmdbDataType)
{
    switch(mmdbDataType)
    {
        case  MMDB_DATA_TYPE_EXTENDED:
            return "extended";
        case  MMDB_DATA_TYPE_POINTER:
            return "pointer";
        case  MMDB_DATA_TYPE_UTF8_STRING:
            return "utf8 string";
        case  MMDB_DATA_TYPE_DOUBLE:
            return "double";
        case  MMDB_DATA_TYPE_BYTES:
            return "bytes";
        case  MMDB_DATA_TYPE_UINT16:
            return "uint16";
        case  MMDB_DATA_TYPE_UINT32:
            return "uint32";
        case  MMDB_DATA_TYPE_MAP:
            return "map";
        case  MMDB_DATA_TYPE_INT32:
            return "int32";
        case  MMDB_DATA_TYPE_UINT64:
            return "uint64";
        case  MMDB_DATA_TYPE_UINT128:
            return "uint128";
        case  MMDB_DATA_TYPE_ARRAY:
            return "array";
        case  MMDB_DATA_TYPE_CONTAINER:
            return "container";
        case  MMDB_DATA_TYPE_END_MARKER:
            return "end marker";
        case  MMDB_DATA_TYPE_BOOLEAN:
            return "boolean";
        case  MMDB_DATA_TYPE_FLOAT:
            return "float";
        default:
            return "unknown";
    }
}

} // namespace nx::maxmind::utils