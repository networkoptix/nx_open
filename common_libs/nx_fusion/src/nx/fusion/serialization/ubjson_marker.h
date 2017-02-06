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
        case NullMarker:
        case TrueMarker:
        case FalseMarker:
        case UInt8Marker:
        case Int8Marker:
        case Int16Marker:
        case Int32Marker:
        case Int64Marker:
        case FloatMarker:
        case DoubleMarker:
        case BigNumberMarker:
        case Latin1CharMarker:
        case Utf8StringMarker:
        case ArrayStartMarker:
        case ArrayEndMarker:
        case ObjectStartMarker:
        case ObjectEndMarker:
        case NoopMarker:
        case ContainerTypeMarker:
        case ContainerSizeMarker:
            return static_cast<Marker>(c);
        default:
            return InvalidMarker;
        }
    }

    inline char charFromMarker(Marker marker) {
        return static_cast<char>(marker);
    }

    inline bool isValidContainerType(Marker marker) {
        switch (marker) {
        case NullMarker:
        case TrueMarker:
        case FalseMarker:
        case UInt8Marker:
        case Int8Marker:
        case Int16Marker:
        case Int32Marker:
        case Int64Marker:
        case FloatMarker:
        case DoubleMarker:
        case BigNumberMarker:
        case Latin1CharMarker:
        case Utf8StringMarker:
            return true;
        default:
            return false;
        }
    }

} // namespace QnUbj

#endif // QN_UBJSON_MARKER_H
