#ifndef QN_PROTO_VALUE_H
#define QN_PROTO_VALUE_H

#include <QtCore/QByteArray>

class QnProtoValue {
public:
    enum Type {
        Variant = 0,
        Fixed64 = 1,
        Fixed32 = 5,
        Bytes = 2,
        Undefined = 0x80
    };

    enum VariantType {
        SignedMask      = 0x10,
        UnsignedMask    = 0x20,

        Int32Variant    = 0x01,
        UInt32Variant   = Int32Variant | UnsignedMask,
        SInt32Variant   = Int32Variant | SignedMask,
        Int64Variant    = 0x02,
        UInt64Variant   = Int64Variant | UnsignedMask,
        SInt64Variant   = Int64Variant | SignedMask,
        BoolVariant     = 0x03,
        EnumVariant     = 0x04,
    };

    QnProtoValue(): m_type(Variant), m_variant(0) {}
    QnProtoValue(quint64 variant, VariantType type): m_type(Variant), m_variant((type & SignedMask) ? toZigzag64(variant) : variant) {}
    
    QnProtoValue(quint64 fixed64): m_type(Fixed64), m_uint64(fixed64) {}
    QnProtoValue(qint64 fixed64): m_type(Fixed64), m_int64(fixed64) {}
    QnProtoValue(double fixed64): m_type(Fixed64), m_double(fixed64) {}
    QnProtoValue(quint32 fixed32): m_type(Fixed32), m_uint32(fixed32) {}
    QnProtoValue(qint32 fixed32): m_type(Fixed32), m_int32(fixed32) {}
    QnProtoValue(float fixed32): m_type(Fixed32), m_float(fixed32) {}
    QnProtoValue(const QByteArray &bytes): m_type(Bytes), m_bytes(bytes) {}

    Type type() const {
        return m_type;
    }

    qint32 toInt32(qint32 defaultValue = 0) const {
        if(m_type == Fixed32) {
            return m_int32;
        } else if(m_type == Variant) {
            return m_variant;
        } else {
            return defaultValue;
        }
    }

    quint32 toUInt32(quint32 defaultValue = 0) const {
        if(m_type == Fixed32) {
            return m_uint32;
        } else if(m_type == Variant) {
            return m_variant;
        } else {
            return defaultValue;
        }
    }

    qint32 toSInt32(qint32 defaultValue = 0) const {
        if(m_type == Fixed32) {
            return m_int32;
        } else if(m_type == Variant) {
            return fromZigzag64(m_variant);
        } else {
            return defaultValue;
        }
    }

    qint64 toInt64(qint64 defaultValue = 0) const {
        if(m_type == Fixed64) {
            return m_int64;
        } else if(m_type == Variant) {
            return m_variant;
        } else {
            return defaultValue;
        }
    }

    quint64 toUInt64(quint64 defaultValue = 0) const {
        if(m_type == Fixed64) {
            return m_uint64;
        } else if(m_type == Variant) {
            return m_variant;
        } else {
            return defaultValue;
        }
    }

    qint64 toSInt64(qint64 defaultValue = 0) const {
        if(m_type == Fixed64) {
            return m_uint64;
        } else if(m_type == Variant) {
            return fromZigzag64(m_variant);
        } else {
            return defaultValue;
        }
    }

    bool toBool(bool defaultValue = false) const {
        if(m_type == Variant) {
            return m_variant;
        } else {
            return defaultValue;
        }
    }

    float toFloat(float defaultValue = 0.0f) const {
        if(m_type == Fixed32) {
            return m_float;
        } else {
            return defaultValue;
        }
    }

    double toDouble(double defaultValue = 0.0) const {
        if(m_type == Fixed64) {
            return m_double;
        } else {
            return defaultValue;
        }
    }

    QByteArray toBytes(const QByteArray &defaultValue = QByteArray()) const {
        if(m_type == Bytes) {
            return m_bytes;
        } else {
            return defaultValue;
        }
    }

private:
    static quint64 toZigzag64(qint64 input) {
        return ((input << 1) ^ (input >> 63));
    }

    static qint64 fromZigzag64(quint64 input) {
        return static_cast<qint64>(((input >> 1) ^ -(static_cast<qint64>(input) & 1)));
    }

    static quint32 toZigzag32(qint32 input) {
        return ((input << 1) ^ (input >> 31));
    }

    static qint32 fromZigzag32(quint32 input) {
        return static_cast<qint32>(((input >> 1) ^ -(static_cast<qint64>(input) & 1)));
    }

private:
    Type m_type;
    union {
        quint64 m_variant;
        quint64 m_uint64;
        qint64 m_int64;
        double m_double;
        quint32 m_uint32;
        qint32 m_int32;
        float m_float;
    };
    QByteArray m_bytes;
};


#endif // QN_PROTO_VALUE_H
