#ifndef __THIRD_PARTY_STORAGE_H__
#define __THIRD_PARTY_STORAGE_H__

#include <cstdint>
#include "plugins/plugin_api.h"

#if defined(_WIN32)
    #define STORAGE_METHOD_CALL __stdcall
#else
    #define STORAGE_METHOD_CALL
#endif

//!Storage plugin namespace
namespace nx_spl
{
    const uint64_t unknown_size = 0xffffffffffffffff;


    //!File IO flags.
    namespace io
    {
        enum mode_t
        {
            NotOpen         = 0x0001,
            ReadOnly        = 0x0002,
            WriteOnly       = 0x0004,
            ReadWrite       = ReadOnly | WriteOnly,
        };
    }

    //!Storage capabilities flags.
    namespace cap
    {
        enum cap_t
        {
            ListFile        = 0x0001,                   // capable of listing files
            RemoveFile      = 0x0002,                   // capable of removing files
            ReadFile        = 0x0004,                   // capable of reading files
            WriteFile       = 0x0008,                   // capable of writing files
            DBReady         = 0x0010,                   // capable of DB hosting
        };
    }

    //!Storage operations errors
    namespace error
    {
        enum code_t
        {
            NoError,
            StorageUnavailable,
            NotEnoughSpace,
            SpaceInfoNotAvailable,
            EndOfFile,
            ReadNotSupported,
            WriteNotSupported,
            UrlNotExists,
            UnknownError,
        };
    }

    enum FileType
    {
        isFile,
        isDir
    };

    // {F922BB26-D59E-4046-9B80-8466B101986B}
    static const nxpl::NX_GUID IID_IODevice =
    { { 0xf9, 0x22, 0xbb, 0x26, 0xd5, 0x9e, 0x40, 0x46, 0x9b, 0x80, 0x84, 0x66, 0xb1, 0x1, 0x98, 0x6b } };

    //!IO device abstraction
    class IODevice
        :public nxpl::PluginInterface
    {
    public:
        /**
        *   \param src      source data
        *   \param size     write size (bytes)
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         bytes written.
        */
        virtual uint32_t STORAGE_METHOD_CALL write(
            const void*     src,
            const uint32_t  size,
            int*            ecode
        ) = 0;

        /**
        *   reads data from device to dst pointer
        *
        *   \param dst      pointer to user buffer
        *   \param size     read size (bytes)
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         bytes read.
        */
        virtual uint32_t STORAGE_METHOD_CALL read(
            void*           dst,
            const uint32_t  size,
            int*            ecode
        ) const = 0;

        /**
        *   \return     IOMode
        */
        virtual int STORAGE_METHOD_CALL getMode() const = 0;

        /**
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         available (read) size of device
        */
        virtual uint32_t STORAGE_METHOD_CALL size(int* ecode) const = 0;

        /**
        *   \param pos      seek position
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         success (0,1)
        */
        virtual int STORAGE_METHOD_CALL seek(
            uint64_t    pos,
            int*        ecode
        ) = 0;
    };

    //! Common file information
    struct FileInfo
    {
        const char* url;
        uint64_t    size;
        FileType    type;
    };

    // {D5DA5C59-44D5-4C14-AB17-EA7395555189}
    static const nxpl::NX_GUID IID_FileInfoIterator =
    { { 0xd5, 0xda, 0x5c, 0x59, 0x44, 0xd5, 0x4c, 0x14, 0xab, 0x17, 0xea, 0x73, 0x95, 0x55, 0x51, 0x89 } };

    //!File information iterator abstraction
    class FileInfoIterator
        : public nxpl::PluginInterface
    {
    public:
        /**
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         current FileInfo* and moves iterator to the next position. Previous FileInfo* is invalidated.
        *                   If no more files, NULL is returned.
        */
        virtual FileInfo* STORAGE_METHOD_CALL next(int* ecode) const = 0;
    };

    // {D5DA5C59-44D5-4C14-AB17-EA7395555189}
    static const nxpl::NX_GUID IID_Storage =
    { { 0xd5, 0xda, 0x5c, 0x59, 0x44, 0xd5, 0x4c, 0x14, 0xab, 0x17, 0xea, 0x73, 0x95, 0x55, 0x51, 0x89 } };

    //!Storage abstraction
    class Storage
        : public nxpl::PluginInterface
    {
    public:
        /**
        *   \return         is storage available at the moment (0,1).
        */
        virtual int STORAGE_METHOD_CALL isAvailable() const = 0;

        /**
        *   \param url      File URL. If file doesn't exists, it is created. This should be NULL terminated utf8 encoded C string.
        *   \param flags    io flag(s)
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         Free space size ('unknown_size' if request can't be accomplished).
        */
        virtual IODevice* STORAGE_METHOD_CALL open(
            const char*     url,
            int             flags,
            int*            ecode
        ) const = 0;

        /**
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         Free space size ('unknown_size' if request can't be accomplished).
        */
        virtual uint64_t STORAGE_METHOD_CALL getFreeSpace(int* ecode) const = 0;

        /**
        *   \param ecode    Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return         Total space size ('unknown_size' if request can't be accomplished).
        */
        virtual uint64_t STORAGE_METHOD_CALL getTotalSpace(int* ecode) const = 0;

        /**
        *   \return         cap flag(s).
        */
        virtual int STORAGE_METHOD_CALL getCapabilities() const = 0;

        /**
         * \param url       File targeted to delete URL. This should be NULL terminated utf8 encoded C string
         * \param ecode     Pointer to error code. Pass NULL if you are not interested in error information.
         */
        virtual void STORAGE_METHOD_CALL removeFile(
            const char* url,
            int*        ecode
        ) = 0;

        /**
         * \param url       Dir targeted to delete URL. This should be NULL terminated utf8 encoded C string
         * \param ecode     Pointer to error code. Pass NULL if you are not interested in error information.
         */
        virtual void STORAGE_METHOD_CALL removeDir(
            const char* url,
            int*        ecode
        ) = 0;

        /**
         * \param oldUrl    File targeted to rename URL. This should be NULL terminated utf8 encoded C string.
         * \param newUrl    File targeted to rename new URL. This should be NULL terminated C string.
         * \param ecode     Pointer to error code. Pass NULL if you are not interested in error information.
         */
        virtual void STORAGE_METHOD_CALL renameFile(
            const char*     oldUrl,
            const char*     newUrl,
            int*            ecode
        ) = 0;

        /**
        *   \param dirUrl   Directory URL. This should be NULL terminated utf8 encoded C string.
        *   \param ecode    Pointer error code. Pass NULL if you are not interested in error information.
        *   \return         Pointer to FileInfoVector or NULL if something bad happened.
        */
        virtual FileInfoIterator* STORAGE_METHOD_CALL getFileIterator(
            const char*     dirUrl,
            int*            ecode
        ) const = 0;

        /**
         * \param url       File URL. This should be NULL terminated utf8 encoded C string
         * \param ecode     Pointer error code. Pass NULL if you are not interested in error information.
         * \return          (0, 1).
         */
        virtual int STORAGE_METHOD_CALL fileExists(
            const char*     url,
            int*            ecode
        ) const = 0;

        /**
         * \param url       Directory URL. This should be NULL terminated utf8 encoded C string
         * \param ecode     Pointer to error code. Pass NULL if you are not interested in error information.
         * \return          (0, 1).
         */
        virtual int STORAGE_METHOD_CALL dirExists(
            const char*     url,
            int*            ecode
        ) const = 0;

        /**
         * \param url       File URL. This should be NULL terminated utf8 encoded C string
         * \param ecode     Pointer to error code. Pass NULL if you are not interested in error information.
         * \return          File size.
         */
        virtual uint64_t STORAGE_METHOD_CALL fileSize(
            const char*     url,
            int*            ecode
        ) const = 0;
    };

    // {2E2C7A3D-256D-4018-B40E-512D72510BEC}
    static const nxpl::NX_GUID IID_StorageFactory =
    { { 0x2e, 0x2c, 0x7a, 0x3d, 0x25, 0x6d, 0x40, 0x18, 0xb4, 0xe, 0x51, 0x2d, 0x72, 0x51, 0xb, 0xec } };

    //!Storage factory abstraction
    /*!
        This is the entry-point library class.
            - Creates storage entities.
            - "Knows" storage library type
    */
    class StorageFactory
        : public nxpl::PluginInterface
    {
    public:
        /**
        *   \return available utf8 encoded urls list. Iterate till NULL.
        */
        virtual const char** STORAGE_METHOD_CALL findAvailable() const = 0;

        /**
        *   \param  url     Storage root URL. This should be NULL terminated utf8 encoded C string.
        *   \param  ecode   Pointer to error code. Pass NULL if you are not interested in error information.
        *   \return pointer to Storage.
        */
        virtual Storage* STORAGE_METHOD_CALL createStorage(
            const char* url,
            int*        ecode
        ) = 0;

        /**
        *   \return ascii string with storage type
        */
        virtual const char* STORAGE_METHOD_CALL storageType() const = 0;

        /**
        *   \param  ecode   Error code
        *   \return         NULL terminated utf8 encoded C string
        */
        virtual const char* lastErrorMessage(int ecode) const = 0;
    };
}

#endif // __THIRD_PARTY_STORAGE_H__
