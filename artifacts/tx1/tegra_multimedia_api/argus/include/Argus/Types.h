/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARGUS_TYPES_H
#define _ARGUS_TYPES_H

/**
 * @file Types.h
 * Defines the basic types that are used by the API.
 */

#include <stdint.h>
#include <vector>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// Some versions of the Xlib.h header file define 'Status' to 'int'. This collides with the Argus
// 'Status' type.
// If 'Status' is defined then undefine it and use a typedef instead.
#ifdef Status
#undef Status
typedef int Status;
#endif // Status

namespace Argus
{

/*
 * Forward declaration of standard objects
 */
class CameraDevice;
class CameraProvider;
class CaptureSession;
class CaptureMetadata;
class CaptureMetadataContainer;
class Event;
class EventQueue;
class InputStream;
class OutputStream;
class OutputStreamSettings;
class Request;
class SensorMode;

/*
 * Forward declaration of standard interfaces
 */
class ICameraProperties;
class ICameraProvider;
class ICaptureSession;
class IAutoControlSettings;
class IRequest;
class IStream;
class IStreamSettings;

/**
 * Constant used for infinite timeouts.
 */
const uint64_t TIMEOUT_INFINITE = 0xFFFFFFFFFFFFFFFF;

/**
 * Extension name UUID. Values are defined in extension headers.
 */
class ExtensionName : public NamedUUID
{
public:
    ExtensionName(uint32_t time_low_
                , uint16_t time_mid_
                , uint16_t time_hi_and_version_
                , uint16_t clock_seq_
                , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5
                , const char* name)
    : NamedUUID(time_low_, time_mid_, time_hi_and_version_, clock_seq_,
                c0, c1, c2, c3, c4, c5, name)
    {}

    ExtensionName()
    : NamedUUID(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "EXT_UNSPECIFIED")
    {}
};

/*
 * Named UUID classes.
 */

DEFINE_NAMED_UUID_CLASS(AwbMode);
DEFINE_NAMED_UUID_CLASS(CaptureIntent);
DEFINE_NAMED_UUID_CLASS(DenoiseMode);
DEFINE_NAMED_UUID_CLASS(EdgeEnhanceMode);
DEFINE_NAMED_UUID_CLASS(SensorModeType);
DEFINE_NAMED_UUID_CLASS(VideoStabilizationMode);

/*
 * Named UUIDs sorted alphabetically.
 */

DEFINE_UUID(AwbMode, AWB_MODE_OFF, FB3F365A,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_AUTO, FB3F365B,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_INCANDESCENT, FB3F365C,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_FLUORESCENT, FB3F365D,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_WARM_FLUORESCENT, FB3F365E,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_DAYLIGHT, FB3F365F,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_CLOUDY_DAYLIGHT, FB3F3660,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_TWILIGHT, FB3F3661,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_SHADE, FB3F3662,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_MANUAL, 20FB45DA,C49F,4293,AB02,13,3F,8C,CA,DD,69);

DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_MANUAL,
                           FB3F3663,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_PREVIEW,
                           FB3F3664,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_STILL_CAPTURE,
                           FB3F3665,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_VIDEO_RECORD,
                           FB3F3666,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_VIDEO_SNAPSHOT,
                           FB3F3667,CC62,11E5,9956,62,56,62,87,07,61);

DEFINE_UUID(DenoiseMode, DENOISE_MODE_OFF, FB3F3668,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(DenoiseMode, DENOISE_MODE_FAST, FB3F3669,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(DenoiseMode, DENOISE_MODE_HIGH_QUALITY, FB3F366A,CC62,11E5,9956,62,56,62,87,07,61);

DEFINE_UUID(EdgeEnhanceMode, EDGE_ENHANCE_MODE_OFF, F7100B40,6A5F,11E6,BDF4,08,00,20,0C,9A,66);
DEFINE_UUID(EdgeEnhanceMode, EDGE_ENHANCE_MODE_FAST, F7100B41,6A5F,11E6,BDF4,08,00,20,0C,9A,66);
DEFINE_UUID(EdgeEnhanceMode, EDGE_ENHANCE_MODE_HIGH_QUALITY, F7100B42,6A5F,11E6,BDF4,08,00,20,0C,9A,66);

DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_DEPTH,
                            64483464,4b91,11e6,bbbd,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_YUV,
                            6453e00c,4b91,11e6,871d,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_RGB,
                            6463d4c6,4b91,11e6,88a3,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_BAYER,
                            646f04ea,4b91,11e6,9c06,40,16,7e,ab,86,92);

DEFINE_UUID(VideoStabilizationMode, VIDEO_STABILIZATION_MODE_OFF,
                                    FB3F366E,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(VideoStabilizationMode, VIDEO_STABILIZATION_MODE_ON,
                                    FB3F366F,CC62,11E5,9956,62,56,62,87,07,61);

/*
 * Camera settings constants - sorted alphabetically.
 */

enum AeAntibandingMode
{
    AE_ANTIBANDING_MODE_OFF,
    AE_ANTIBANDING_MODE_AUTO,
    AE_ANTIBANDING_MODE_50HZ,
    AE_ANTIBANDING_MODE_60HZ,

    AE_ANTIBANDING_MODE_COUNT
};

enum AeState
{
    AE_STATE_INACTIVE,
    AE_STATE_SEARCHING,
    AE_STATE_CONVERGED,
    AE_STATE_FLASH_REQUIRED,
    AE_STATE_LOCKED,

    AE_STATE_COUNT
};

enum AwbState
{
    AWB_STATE_INACTIVE,
    AWB_STATE_SEARCHING,
    AWB_STATE_CONVERGED,
    AWB_STATE_LOCKED,

    AWB_STATE_COUNT
};

enum BayerChannel
{
    BAYER_CHANNEL_R,
    BAYER_CHANNEL_G_EVEN,
    BAYER_CHANNEL_G_ODD,
    BAYER_CHANNEL_B,

    BAYER_CHANNEL_COUNT
};

/**
 * Status values returned by API function calls.
 */
enum Status
{
    /// Function succeeded.
    STATUS_OK,

    /// The set of parameters passed was invalid.
    STATUS_INVALID_PARAMS,

    /// The requested settings are invalid.
    STATUS_INVALID_SETTINGS,

    /// The requested device is unavailable.
    STATUS_UNAVAILABLE,

    /// An operation failed because of insufficient mavailable memory.
    STATUS_OUT_OF_MEMORY,

    /// This method has not been implemented.
    STATUS_UNIMPLEMENTED,

    /// An operation timed out.
    STATUS_TIMEOUT,

    /// The capture was aborted. @see ICaptureSession::cancelRequests()
    STATUS_CANCELLED,

    /// The stream or other resource has been disconnected.
    STATUS_DISCONNECTED,

    // Number of elements in this enum.
    STATUS_COUNT
};

enum RGBColorChannel
{
    RGB_CHANNEL_RED,
    RGB_CHANNEL_GREEN,
    RGB_CHANNEL_BLUE,

    RGB_CHANNEL_COUNT
};


/**
 * Pixel formats.
 */
class PixelFormat : public NamedUUID
{
public:
    PixelFormat(uint32_t time_low_
              , uint16_t time_mid_
              , uint16_t time_hi_and_version_
              , uint16_t clock_seq_
              , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5
              , const char* name)
    : NamedUUID(time_low_, time_mid_, time_hi_and_version_, clock_seq_,
                c0, c1, c2, c3, c4, c5, name)
    {}

private:
    PixelFormat(); // No default c'tor please use PIXEL_FMT_UNKNOWN
};

DEFINE_UUID(PixelFormat, PIXEL_FMT_UNKNOWN,       00000000,93d5,11e5,0000,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_Y8,            569be14a,93d5,11e5,91bc,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_Y16,           56ddb19c,93d5,11e5,8e2c,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_YCbCr_420_888, 570c10e6,93d5,11e5,8ff3,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_YCbCr_422_888, 573a7940,93d5,11e5,99c2,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_YCbCr_444_888, 576043dc,93d5,11e5,8983,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_JPEG_BLOB,     578b08c4,93d5,11e5,9686,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_RAW16,         57b484d8,93d5,11e5,aeb6,1c,b7,2c,ef,d4,1e);

/**
 * Utility class for Argus interfaces.
 */
class NonCopyable
{
protected:
    NonCopyable() {}

private:
    NonCopyable(NonCopyable& other);
    NonCopyable& operator=(NonCopyable& other);
};

/**
 * The top-level interface class.
 *
 * By convention, every Interface subclass exposes a public static method called @c id(),
 * which returns the unique InterfaceID for that interface.
 * This is required for the @c interface_cast<> template to work with that interface.
 */
class Interface : NonCopyable
{
protected:
    Interface() {}
    ~Interface() {}
};

/**
 * A unique identifier for an Argus Interface.
 */
class InterfaceID : public NamedUUID
{
public:
    InterfaceID(uint32_t time_low_
              , uint16_t time_mid_
              , uint16_t time_hi_and_version_
              , uint16_t clock_seq_
              , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5
              , const char* name)
    : NamedUUID(time_low_, time_mid_, time_hi_and_version_, clock_seq_,
                c0, c1, c2, c3, c4, c5, name)
    {}

    InterfaceID()
    : NamedUUID(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "IID_UNSPECIFIED")
    {}
};

/**
 * The base interface for a class that provides Argus Interfaces.
 */
class InterfaceProvider : NonCopyable
{
public:

    /**
     * Acquire the interface specified by @c interfaceId.
     * @returns An instance of the requested interface,
     * or NULL if that interface is not available.
     */
    virtual Interface* getInterface(const InterfaceID& interfaceId) = 0;

protected:
    ~InterfaceProvider() {}
};

/**
 * Interface-casting helper similar to dynamic_cast.
 */

template <typename TheInterface>
inline TheInterface* interface_cast(InterfaceProvider* obj)
{
    return static_cast<TheInterface*>(obj ? obj->getInterface(TheInterface::id()): 0);
}

template <typename TheInterface>
inline TheInterface* interface_cast(const InterfaceProvider* obj)
{
    return static_cast<TheInterface*>(
        obj ? const_cast<const Interface*>(
                const_cast<InterfaceProvider*>(obj)->getInterface(TheInterface::id())): 0);
}

/**
 * A top level object class for Argus objects that are created and owned by
 * the client. All Destructable objects created by the client must be explicitly destroyed.
 */
class Destructable
{
public:

    /**
     * Destroy this object.
     * After making this call, the client cannot make any more calls on this object.
     */
    virtual void destroy() = 0;

protected:
    ~Destructable() {}
};

/**
 * Template helper emulating C++11 rvalue semantics.
 */
template<typename T>
class rv : public T
{
    rv();
    ~rv();
    rv(const rv&);
    void operator=(const rv&);
};

template<typename T>
    rv<T>& move(T& self)
{
    return *static_cast<rv<T>*>(&self);
}

/**
 * Movable smart pointer mimicking std::unique_ptr.
 */
template <typename T> struct remove_const;
template <typename T> struct remove_const<const T&>{ typedef T& type; };
template <typename T> struct remove_const<const T*>{ typedef T* type; };
template <typename T> struct remove_const<const T >{ typedef T  type; };
template <typename T> struct remove_const          { typedef T  type; };

template <typename T>
class UniqueObj : NonCopyable
{
public:
    explicit UniqueObj(T* obj=NULL): m_obj(obj) {}

    void reset(T* obj=NULL)
        { if (m_obj) const_cast<typename remove_const<T*>::type>(m_obj)->destroy(); m_obj = obj; }
    T* release()
        { T* obj = m_obj; m_obj = NULL; return obj; }

    UniqueObj( rv<UniqueObj>& moved ): m_obj(moved.release()) {}
    UniqueObj& operator=( rv<UniqueObj>& moved ){ reset( moved.release()); return *this; }

    ~UniqueObj() { reset(); }

    T& operator*() const { return *m_obj; }
    T* get() const { return m_obj; }

    operator bool() const { return !!m_obj; }

    operator       rv<UniqueObj>&()       { return *static_cast<      rv<UniqueObj>*>(this); }
    operator const rv<UniqueObj>&() const { return *static_cast<const rv<UniqueObj>*>(this); }

private:
    T* m_obj;

    T* operator->() const; // Prevent calling destroy() directly.
                           // Note: For getInterface functionality use interface_cast.
};

template <typename TheInterface, typename TObject>
inline TheInterface* interface_cast(const UniqueObj<TObject>& obj)
{
    return interface_cast<TheInterface>( obj.get());
}

/**
 * A templatized class to hold a min/max range of values.
 */
template <typename T>
struct Range
{
    T min;
    T max;

    Range(T value) : min( value ), max( value ) {}
    Range(T min, T max) : min( min ), max( max ) {};
    bool operator== (const Range<T>& rhs) const
    {
        return (min == rhs.min && max == rhs.max);
    }

    bool empty() const
    {
        return max < min;
    }
};

/**
 * Defines a rectangle in pixel space.
 */
struct Rectangle
{
    uint32_t left;
    uint32_t top;
    uint32_t width;
    uint32_t height;

    Rectangle() : left(0), top(0), width(0), height(0) {}
    Rectangle(uint32_t _left, uint32_t _top, uint32_t _width, uint32_t _height)
        : left(_left), top(_top), width(_width), height(_height)
    {}

    bool operator==(const Rectangle& other) const
    {
        return (left == other.left)   && (top == other.top) &&
               (width == other.width) && (height == other.height);
    }
};

/**
 * Aggregates width and height in a single structure.
 */
struct Size
{
    uint32_t width;
    uint32_t height;

    Size() : width(0), height(0) {}

    /// Construct size from width and height
    Size(uint32_t w, uint32_t h) : width(w), height(h) {}

    bool operator==(const Size& a) const
    {
        return width == a.width && height == a.height;
    }

    Size operator+(const Size& b) const
    {
        return Size(width + b.width, height + b.height);
    }

    Size operator-(const Size& b) const
    {
        return Size(width - b.width, height - b.height);
    }
};

/**
 * Aggregates 2D co-ordinates of a location in a single structure.
 */
struct Location
{
    uint32_t x;
    uint32_t y;

    Location() : x(0), y(0) {}

    /// Construct location from x and y
    Location(uint32_t a, uint32_t b) : x(a), y(b) {}

    bool operator==(const Location& other) const
    {
        return x == other.x && y == other.y;
    }
};

/**
 * Defines a normalized rectangle region in [0.0, 1.0].
 */
struct NormalizedRect
{
    float left;
    float top;
    float right;
    float bottom;

    NormalizedRect() : left(0.0f), top(0.0f), right(1.0f), bottom(1.0f) {}
    NormalizedRect(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
    bool operator==(const NormalizedRect& other) const
    {
        return (left   == other.left) &&
               (top    == other.top) &&
               (right  == other.right) &&
               (bottom == other.bottom);
    }
};

/**
 * Tuple template class. This provides a finite ordered list of N elements having type T.
 */
template <unsigned int N, typename T>
struct Tuple
{
    T data[N];

    bool operator==(const Tuple<N,T>& other) const {
        return !memcmp(data, other.data, sizeof(data));
    }

    T& operator[](unsigned int i)             { assert(i < N); return data[i]; }
    const T& operator[](unsigned int i) const { assert(i < N); return data[i]; }
};

/**
 * BayerTuple template class. This is a Tuple specialization containing 4 elements corresponding
 * to the Bayer color channels: R, G_EVEN, G_ODD, and B. Values can be accessed using the named
 * methods or subscript indexing using the Argus::BayerChannel enum.
 */
template <typename T>
struct BayerTuple : public Tuple<BAYER_CHANNEL_COUNT, T>
{
    BayerTuple() {}
    BayerTuple(T r, T gEven, T gOdd, T b)
    {
        Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_R] = r;
        Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_G_EVEN] = gEven;
        Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_G_ODD] = gOdd;
        Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_B] = b;
    }

    T& r()                 { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_R]; }
    const T& r() const     { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_R]; }
    T& gEven()             { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_G_EVEN]; }
    const T& gEven() const { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_G_EVEN]; }
    T& gOdd()              { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_G_ODD]; }
    const T& gOdd() const  { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_G_ODD]; }
    T& b()                 { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_B]; }
    const T& b() const     { return Tuple<BAYER_CHANNEL_COUNT, T>::data[BAYER_CHANNEL_B]; }
};

/**
 * Defines an autocontrol region of interest (in pixel space).
 */
struct AcRegion
{
    AcRegion()
        : rect(0, 0, 0, 0)
        , weight(1.0f)
    {}

    AcRegion(uint32_t _left, uint32_t _top, uint32_t _width, uint32_t _height, float _weight)
        : rect(_left, _top, _width, _height)
        , weight(_weight)
    {}

    bool operator==(const AcRegion& other) const
    {
        return (weight == other.weight) &&
               (rect == other.rect);
    }

    Rectangle rect;
    float     weight;
};

/**
 * A template class to hold a 2-dimensional array of data.
 * Data in this array is tightly packed in a 1-dimensional vector in row-major order;
 * that is, the vector index for any value given its 2-dimensional Location is
 *  index = location.x + (location.y * size.x);
 * Indexing operators using iterators, 1-dimensional, or 2-dimensional coordinates are provided.
 */
template <typename T>
class Array2D
{
public:
    // Iterator types.
    typedef T* iterator;
    typedef const T* const_iterator;

    /// Default Constructor.
    Array2D() : m_size(0, 0) {}

    /// Constructor given initial array size.
    Array2D(Size size) : m_size(size)
    {
        m_data.resize(size.width * size.height);
    }

    /// Constructor given initial array size and initial fill value.
    Array2D(Size size, const T& value) : m_size(size)
    {
        m_data.resize(size.width * size.height, value);
    }

    /// Copy constructor.
    Array2D(const Array2D<T>& other)
    {
        m_data = other.m_data;
        m_size = other.m_size;
    }

    /// Assignment operator.
    Array2D& operator= (const Array2D<T>& other)
    {
        m_data = other.m_data;
        m_size = other.m_size;
        return *this;
    }

    /// Equality operator.
    bool operator== (const Array2D<T>& other) const
    {
        return (m_size == other.m_size && m_data == other.m_data);
    }

    /// Returns the size (dimensions) of the array.
    Size size() const { return m_size; }

    /// Resize the array. Array contents after resize are undefined.
    /// Boolean return value enables error checking when exceptions are not available.
    bool resize(Size size)
    {
        uint32_t s = size.width * size.height;
        m_data.resize(s);
        if (m_data.size() != s)
            return false;
        m_size = size;
        return true;
    }

    /// STL style iterators.
    inline const_iterator begin() const { return m_data.data(); }
    inline const_iterator end() const { return m_data.data() + m_data.size(); }
    inline iterator begin() { return m_data.data(); }
    inline iterator end() { return m_data.data() + m_data.size(); }

    /// Array indexing using [] operator.
    T& operator[](unsigned int i) { return m_data[checkIndex(i)]; }
    const T& operator[](unsigned int i) const { return m_data[checkIndex(i)]; }

    /// Array indexing using () operator.
    inline const T& operator() (uint32_t i) const { return m_data[checkIndex(i)]; }
    inline const T& operator() (uint32_t x, uint32_t y) const { return m_data[checkIndex(x, y)]; }
    inline const T& operator() (Location l) const { return m_data[checkIndex(l.x, l.y)]; }
    inline T& operator() (uint32_t i) { return m_data[checkIndex(i)]; }
    inline T& operator() (uint32_t x, uint32_t y) { return m_data[checkIndex(x, y)]; }
    inline T& operator() (Location l) { return m_data[checkIndex(l.x, l.y)]; }

    // Get pointers to data.
    inline const T* data() const { return m_data.data(); }
    inline T* data() { return m_data.data(); }

private:
    inline uint32_t checkIndex(uint32_t i) const
    {
        assert(i < m_data.size());
        return i;
    }

    inline uint32_t checkIndex(uint32_t x, uint32_t y) const
    {
        assert(x < m_size.width);
        assert(y < m_size.height);
        return x + (y * m_size.width);
    }

    std::vector<T> m_data;
    Size m_size;
};

typedef uint32_t AutoControlId;

} // namespace Argus

#endif // _ARGUS_TYPES_H
