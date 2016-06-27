#ifndef QN_ALIGNED_ALLOCATOR_H
#define QN_ALIGNED_ALLOCATOR_H

#include <memory>

#include <nx/streaming/config.h>

template <typename T, std::size_t N = CL_MEDIA_ALIGNMENT>
class QnAlignedAllocator {
public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    typedef T * pointer;
    typedef const T * const_pointer;

    typedef T & reference;
    typedef const T & const_reference;

public:
    inline QnAlignedAllocator() throw() {}

    template <typename T2>
    inline QnAlignedAllocator(const QnAlignedAllocator<T2, N> &) throw() {}

    inline ~QnAlignedAllocator() throw() {}

    inline pointer address(reference r) {
        return &r;
    }

    inline const_pointer address(const_reference r) const {
        return &r;
    }

    inline pointer allocate(size_type n) {
        return (pointer) qMallocAligned(n * sizeof(value_type), N);
    }

    inline void deallocate(pointer p, size_type) {
        qFreeAligned(p);
    }

    inline void construct(pointer p, const value_type &wert) {
        new (p) value_type(wert);
    }

    /**
     * C++11 extension member.
     */
    inline void construct(pointer p) {
        new (p) value_type();
    }

    inline void destroy(pointer p) {
        static_cast<void>(p);
        p->~value_type();
    }

    inline size_type max_size() const throw() {
        return size_type(-1) / sizeof(value_type);
    }

    template <typename T2>
    struct rebind {
        typedef QnAlignedAllocator<T2, N> other;
    };

    inline bool operator!=(const QnAlignedAllocator<T, N> &other) const  {
        return !(*this == other);
    }

    /**
     * \param other
     * \returns                         True if and only if storage allocated from \a this can be deallocated from \a other.
     *                                  Always returns true for stateless allocators.
     */
    inline bool operator==(const QnAlignedAllocator<T,N> &other) const {
        Q_UNUSED(other)
        return true;
    }
};

#endif // QN_ALIGNED_ALLOCATOR_H
