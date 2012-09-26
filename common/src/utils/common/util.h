#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

#include "config.h"

#include <QtCore/QString>

#include "math.h" /* For INT64_MAX. */

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

/**
 * Remove directory recursively.
 * 
 * \param dirName                       Name of the directory to remove.
 * \returns                             Whether the operation completer successfully.
 */
QN_EXPORT bool removeDir(const QString &dirName);

/**
 * Convert path from native to universal.
 * 
 * \param path                          Path to convert.
 * \returns                             Converted path.
 */
QN_EXPORT QString fromNativePath(QString path);

/**
 * \returns                             User movies directory.
 */
QN_EXPORT QString getMoviesDirectory();

/**
 * \param num                           Number.
 * \returns                             Number of digits in decimal representation of the given number.
 */
QN_EXPORT int digitsInNumber(unsigned num);

/**
 * Formats position in a time interval using the "HH:MM:SS/HH:MM:SS" format.
 * Hours part is omitted when it is not relevant.
 *
 * \param position                      Current position in seconds.
 * \param total                         Total duration in seconds. 
 *                                      Pass zero to convert position only.
 */
QN_EXPORT QString formatDuration(unsigned position, unsigned total = 0);

/**
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QN_EXPORT QString getParamFromString(const QString& str, const QString& param);

QN_EXPORT QString strPadLeft(const QString &str, int len, char ch);

QN_EXPORT QString closeDirPath(const QString& value);

QN_EXPORT qint64 getDiskFreeSpace(const QString& root);

#define DATETIME_NOW INT64_MAX 

#define DEFAULT_APPSERVER_HOST "127.0.0.1"
#define DEFAULT_APPSERVER_PORT 7001

#define MAX_RTSP_DATA_LEN (65535 - 16)

#define BACKWARD_SEEK_STEP (1000ll * 1000)

// This constant limit duration of a first GOP. So, if jump to position X, and first I-frame has position X-N, data will be transfer for range
// [X-N..X-N+MAX_FIRST_GOP_DURATION]. It prevent long preparing then reverse mode is activate.
// I have increased constant because of camera with very low FPS may have long gop (30-60 seconds).
//static const qint64 MAX_FIRST_GOP_DURATION = 1000 * 1000 * 10;
#define MAX_FIRST_GOP_FRAMES 250ll

#define MAX_FRAME_DURATION (5ll * 1000)
#define MIN_FRAME_DURATION 16667ll
#define MAX_AUDIO_FRAME_DURATION 150ll

quint64 QN_EXPORT getUsecTimer();

template <typename T, std::size_t N = CL_MEDIA_ALIGNMENT>
class AlignmentAllocator {
public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    typedef T * pointer;
    typedef const T * const_pointer;

    typedef T & reference;
    typedef const T & const_reference;

public:
    inline AlignmentAllocator() noexcept {}

    template <typename T2>
    inline AlignmentAllocator(const AlignmentAllocator<T2, N> &) noexcept {}

    inline ~AlignmentAllocator() noexcept {}

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
        (void) p; /* Silence MSVC unused variable warnings. */

        p->~value_type();
    }

    inline size_type max_size() const noexcept {
        return size_type(-1) / sizeof(value_type);
    }

    template <typename T2>
    struct rebind {
        typedef AlignmentAllocator<T2, N> other;
    };

    bool operator!=(const AlignmentAllocator<T, N> &other) const  {
        return !(*this == other);
    }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    // Always returns true for stateless allocators.
    bool operator==(const AlignmentAllocator<T,N>& other) const {
        return true;
    }
};



#endif // _UNIVERSAL_CLIENT_UTIL_H
