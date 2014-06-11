#ifndef QN_UBJ_MARKER_H
#define QN_UBJ_MARKER_H

namespace QnUbj {
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

    QnUbj::Marker markerFromChar(char c) {
        switch (c) {
        case 'Z': case 'T': case 'F': case 'U': case 'i': case 'I': case 'l':
        case 'L': case 'd': case 'D': case 'H': case 'C': case 'S':
        case '[': case ']': case '{': case '}':
        case 'N': case '$': case '#':
            return static_cast<QnUbj::Marker>(c);
        default:    
            return QnUbj::InvalidMarker;
        }
    }

    char charFromMarker(Marker marker) {
        return static_cast<char>(marker);
    }

    bool isValidContainerType(Marker marker) {
        switch (marker) {
        case QnUbj::NullMarker:
        case QnUbj::TrueMarker:
        case QnUbj::FalseMarker:
        case QnUbj::UInt8Marker:
        case QnUbj::Int8Marker:
        case QnUbj::Int16Marker:
        case QnUbj::Int32Marker:
        case QnUbj::Int64Marker:
        case QnUbj::FloatMarker:
        case QnUbj::DoubleMarker:
        case QnUbj::BigNumberMarker:
        case QnUbj::Latin1CharMarker:
        case QnUbj::Utf8StringMarker:
            return true;
        default:
            return false;
        }
    }

} // namespace QnUbj

#endif // QN_UBJ_MARKER_H
