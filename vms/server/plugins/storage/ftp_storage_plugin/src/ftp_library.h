// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * This is the example project aimed to demonstrate a possible implementation of a Storage plugin.
 * - nx_spl::StorageFactory::createStorage() function will receive url to base folder.
 *     For example: `ftp://10.10.2.54/some/path`
 * - All other API functions which have URL parameter will receive URL without scheme and host.
 *     For example: `/some/path/folder1/folder2`
 * - In FileInfoIterator::next() the plugin should provide full URL to target location (with or
 *     without scheme + host).
 *     For example:
 *     <pre><code>
 *     fileInfo.url = ftp://10.10.2.54/some/path/file1.mkv // right
 *     fileInfo.url = /some/path/file1.mkv // right
 *     fileInfo.url = file1.mkv // wrong
 *     </code></pre>
 * - Some of the API functions are called frequently, for example open(), some - based on some
 *     (sometimes quite long) timeout, for example, removeDir(). But the Server calls all of them
 *     in time, so every function should be implemented correctly.
 * - For some storages types, for example FTP, it may be impossible to 'honestly' implement such
 *     fuctions as getTotalSpace() and getFreeSpace(). In this case these functions are allowed to
 *     return some sensible constant value. Keep in mind that getTotalSpace() is used mainly in
 *     determining "best" storage algorithm. For example if some storage total space is far smaller
 *     than others this storage won't be selected for writing. Or if some storage is twice larger
 *     than another, it will be written on twice more data. Also total space value is displayed in
 *     client for every storage in list. getFreeSpace() is used to rotate data on storages. If the
 *     Server sees that some storage free space becomes lower than predefined limit (5 GB by
 *     default) it will try to remove some old files and folders on this storage.
 *
 * Usage of the plugin:
 *
 * Connect to the Server with a Client. In external storage selection dialog you should be able to
 * see new storage type (FTP). Enter a valid ftp url and credentials and press Ok. For example:
 * - Url: `ftp://10.2.3.87/path/to/storage`
 * - Login: `user1`
 * - Password: `12345678`
 */

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <stdint.h>

#include "ftplibpp/ftplib.h"
#include "storage/third_party_storage.h"

namespace nx_spl
{
    namespace aux
    {   // Generic reference counter Mix-In. Private inherit it.
        template <typename P>
        class PluginRefCounter
        {
        public:
            PluginRefCounter()
                : m_count(1)
            {}

            int p_addRef() const { return ++m_count; }

            int p_releaseRef() const
            {
                int new_count = --m_count;
                if (new_count <= 0) {
                    delete static_cast<const P*>(this);
                }
                return new_count;
            }
        private:
            mutable std::atomic<int> m_count;
        }; // class PluginRefCounter

        class NonCopyable
        {
        public:
            NonCopyable() {}
            NonCopyable(const NonCopyable&);
            NonCopyable& operator =(const NonCopyable&);

            NonCopyable(NonCopyable&&);
            NonCopyable& operator =(NonCopyable&&);
        }; // class NonCopyable

        /**
         * A file name and a full path, containing this name. When we communicate with the remote
         * FTP server, * we operate bare file names. But locally we need to map this file to a full
         * path. That's why it's convenient to have both.
         */
        struct FileNameAndPath
        {
            std::string name;
            std::string fullPath;
        };

    } //namespace aux

    typedef std::shared_ptr<ftplib> implPtrType;
    class FtpStorage;
    // At construction phase we synchronise remote file with local one.
    // During destruction synchronisation attempt is repeated.
    // All intermediate actions (read/write/seek) are made with the local copy.
    class FtpIODevice
        : public IODevice,
          private aux::NonCopyable,
          private aux::PluginRefCounter<FtpIODevice>
    {
        friend class aux::PluginRefCounter<FtpIODevice>;
    public:
        FtpIODevice(
            const char         *uri,
            int                 mode,
            const std::string  &storageUrl,
            const std::string  &uname,
            const std::string  &upasswd
        );

        virtual uint32_t STORAGE_METHOD_CALL write(
            const void*     src,
            const uint32_t  size,
            int*            ecode
        ) override;

        virtual uint32_t STORAGE_METHOD_CALL read(
            void*           dst,
            const uint32_t  size,
            int*            ecode
        ) const override;

        virtual int STORAGE_METHOD_CALL seek(
            uint64_t    pos,
            int*        ecode
        ) override;

        virtual int      STORAGE_METHOD_CALL getMode() const override;
        virtual uint32_t STORAGE_METHOD_CALL size(int* ecode) const override;

    public: // plugin interface implementation
        virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

        virtual int addRef() const override;
        virtual int releaseRef() const override;

    private:
        // synchronize localfile with remote one
        void flush();
        // delete only via releaseRef()
        ~FtpIODevice();

    private:
        int                 m_mode;
        mutable int64_t     m_pos;
        std::string         m_uri; //file URI
        implPtrType         m_impl;
        aux::FileNameAndPath     m_localfile;
        bool                m_altered;
        long long           m_localsize;
        mutable
        std::mutex          m_mutex;
        std::string         m_implurl;
        std::string         m_user;
        std::string         m_passwd;
    }; // class FtpIODevice

    // Fileinfo list is obtained from the server at construction phase.
    // After this phase there are no real interactions with FTP server.
    class FtpFileInfoIterator
        : public FileInfoIterator,
          private aux::NonCopyable,
          private aux::PluginRefCounter<FtpFileInfoIterator>
    {
        friend class aux::PluginRefCounter<FtpFileInfoIterator>;

        typedef std::vector<std::string>        FileListType;
        typedef FileListType::const_iterator    FileListIteratorType;
    public:
        FtpFileInfoIterator(
            ftplib              &impl,
            FileListType       &&fileList, // caller doesn't really need this list after Iterator is constructed
            const std::string   &baseDir
        );

        virtual FileInfo* STORAGE_METHOD_CALL next(int* ecode) const override;

    public: // plugin interface implementation
        virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

        virtual int addRef() const override;
        virtual int releaseRef() const override;

    private:
        // In response to MLSD request list of structured file dsecription lines is returned.
        // In this function we try to parse this line and get information we need.
        int fileInfoFromMLSDString(
            const char  *mlsd,  // In. Null-terminated.
            FileInfo    *fi,    // Out
            char        *urlBuf // Out. Should be preallocated.
        ) const;
        // delete only with releaseRef()
        ~FtpFileInfoIterator();

    private:
        mutable std::vector<char>   m_urlData;
        mutable FileInfo            m_fileInfo;
        ftplib&                     m_impl;
        FileListType                m_fileList;
        mutable
        FileListIteratorType        m_curFile;
        int                         m_basedirsize;
    }; // class FtpFileListIterator

    class FtpStorage
        : public Storage,
          private aux::NonCopyable,
          private aux::PluginRefCounter<FtpStorage>
    {
        friend class aux::PluginRefCounter<FtpStorage>;
        // we need pointer because 'ftplib' default constructor can throw
        // and we want to handle it explicitly.
    public: // ctors, helper functions
        FtpStorage(const std::string& url);
        int getAvail() const {return m_available;}

    public: // Storage interface implementation
        virtual int STORAGE_METHOD_CALL isAvailable() const override;

        virtual IODevice* STORAGE_METHOD_CALL open(
            const char*     uri,
            int             flags,
            int*            ecode
        ) const override;

        virtual uint64_t STORAGE_METHOD_CALL getFreeSpace(int* ecode) const override;
        virtual uint64_t STORAGE_METHOD_CALL getTotalSpace(int* ecode) const override;
        virtual int STORAGE_METHOD_CALL getCapabilities() const override;

        virtual void STORAGE_METHOD_CALL removeFile(
            const char* url,
            int*        ecode
        ) override;

        virtual void STORAGE_METHOD_CALL removeDir(
            const char* url,
            int*        ecode
        ) override;

        virtual void STORAGE_METHOD_CALL renameFile(
            const char*     oldUrl,
            const char*     newUrl,
            int*            ecode
        ) override;

        virtual FileInfoIterator* STORAGE_METHOD_CALL getFileIterator(
            const char*     dirUrl,
            int*            ecode
        ) const override;

        virtual int STORAGE_METHOD_CALL fileExists(
            const char*     url,
            int*            ecode
        ) const override;

        virtual int STORAGE_METHOD_CALL dirExists(
            const char*     url,
            int*            ecode
        ) const override;

        virtual uint64_t STORAGE_METHOD_CALL fileSize(
            const char*     url,
            int*            ecode
        ) const override;

    public: // plugin interface implementation
        virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

        virtual int addRef() const override;
        virtual int releaseRef() const override;

    private:
        // destroy only via releaseRef()
        ~FtpStorage();

    private:
        implPtrType         m_impl;
        std::string         m_implurl;
        std::string         m_user;
        std::string         m_passwd;
        mutable std::mutex  m_mutex;
        mutable int         m_available;
    }; // class Ftpstorage

    class FtpStorageFactory
        : public StorageFactory,
          private aux::NonCopyable,
          private aux::PluginRefCounter<FtpStorageFactory>
    {
        friend class aux::PluginRefCounter<FtpStorageFactory>;
    public:
        FtpStorageFactory();
        // currently unimplemented
        virtual const char** STORAGE_METHOD_CALL findAvailable() const override;

        virtual Storage* STORAGE_METHOD_CALL createStorage(
            const char* url,
            int*        ecode
        ) override;

        virtual const char* STORAGE_METHOD_CALL storageType() const override;
        virtual const char* lastErrorMessage(int ecode) const override;

    public: // plugin interface implementation
        virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

        virtual int addRef() const override;
        virtual int releaseRef() const override;
    }; // class FtpStorageFactory
} // namespace Qn
