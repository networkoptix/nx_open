#ifndef QN_UBJSON_MARKER_H
#define QN_UBJSON_MARKER_H

#include "ubjson_fwd.h"

namespace QnUbjson {
    enum Marker {
        NullMarker          = 'Z',
        TrueMarker          = 'T',
        FalseMarker         = 'F',
        UInt8Marker         = 'U',
        Int8Marker          = 'i',
        Int16Marker         = 'I',
        Int32Marker         = 'l',
        Int64Marker         = 'L',
        FloatMarker         = 'd',
        DoubleMarker        = 'D',
        BigNumberMarker     = 'H',
        Latin1CharMarker    = 'C',
        Utf8StringMarker    = 'S',
        ArrayStartMarker    = '[',
        ArrayEndMarker      = ']',
        ObjectStartMarker   = '{',
        ObjectEndMarker     = '}',

        /* The following markers are abstracted away by the reader. They can be 
         * found in the stream, but they will never be returned by the reader 
         * when processing a valid ubjson stream. */

        NoopMarker          = 'N',
        ContainerTypeMarker = '$',
        ContainerSizeMarker = '#',

        InvalidMarker       = '\0'
    };

    inline QnUbjson::Marker markerFromChar(char c) {
        switch (c) {
        case 'Z': case 'T': case 'F': case 'U': case 'i': case 'I': case 'l':
        case 'L': case 'd': case 'D': case 'H': case 'C': case 'S':
        case '[': case ']': case '{': case '}':
        case 'N': case '$': case '#':
            return static_cast<QnUbjson::Marker>(c);
        default:    
            return QnUbjson::InvalidMarker;
        }
    }

    inline char charFromMarker(Marker marker) {
        return static_cast<char>(marker);
    }

    inline bool isValidContainerType(Marker marker) {
        switch (marker) {
        case QnUbjson::NullMarker:
        case QnUbjson::TrueMarker:
        case QnUbjson::FalseMarker:
        case QnUbjson::UInt8Marker:
        case QnUbjson::Int8Marker:
        case QnUbjson::Int16Marker:
        case QnUbjson::Int32Marker:
        case QnUbjson::Int64Marker:
        case QnUbjson::FloatMarker:
        case QnUbjson::DoubleMarker:
        case QnUbjson::BigNumberMarker:
        case QnUbjson::Latin1CharMarker:
        case QnUbjson::Utf8StringMarker:
            return true;
        default:
            return false;
        }
    }

} // namespace QnUbj

#endif // QN_UBJSON_MARKER_H
