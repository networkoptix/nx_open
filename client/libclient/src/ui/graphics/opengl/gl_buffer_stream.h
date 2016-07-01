#ifndef QN_GL_BUFFER_STREAM_H
#define QN_GL_BUFFER_STREAM_H

#include <cassert>

#include <QtCore/QByteArray>
#include <QtGui/QColor>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <ui/common/color_to_vector_converter.h>

template<class T>
class QnGlBufferStream {
public:
    QnGlBufferStream(QByteArray *target): 
        m_target(target) 
    {
        NX_ASSERT(target);

        m_target->reserve(1024);
    }

    int offset() const {
        return m_target->size();
    }

    friend QnGlBufferStream<T> &operator<<(QnGlBufferStream<T> &stream, const T &value) {
        stream.m_target->resize(stream.m_target->size() + sizeof(T));

        *reinterpret_cast<T *>(&*stream.m_target->end() - sizeof(T)) = value;

        return stream;
    }

private:
    QByteArray *m_target;
};

inline QnGlBufferStream<float> &operator<<(QnGlBufferStream<float> &stream, qreal value) {
    return stream << static_cast<float>(value);
}

inline QnGlBufferStream<float> &operator<<(QnGlBufferStream<float> &stream, const QVector2D &value) {
    return stream << value.x() << value.y();
}

inline QnGlBufferStream<float> &operator<<(QnGlBufferStream<float> &stream, const QVector3D &value) {
    return stream << value.x() << value.y() << value.z();
}

inline QnGlBufferStream<float> &operator<<(QnGlBufferStream<float> &stream, const QVector4D &value) {
    return stream << value.x() << value.y() << value.z() << value.w();
}

inline QnGlBufferStream<float> &operator<<(QnGlBufferStream<float> &stream, const QColor &value) {
    return stream << convert<QVector4D>(value);
}

#endif // QN_GL_BUFFER_STREAM_H
