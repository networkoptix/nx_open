#ifndef __FTP_THIRD_PARTY_LIBRARY_H__
#define __FTP_THIRD_PARTY_LIBRARY_H__

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <mutex>
#include "plugins/storage/third_party/third_party_storage.h"
#include "impl/ftplib.h"

/*! \mainpage

    \section intro_sec Introduction
    This is the example project aimed to demonstrate NX Storage plugin API SDK possible implementation.
    \subsection tips_subsec Some tips
    - nx_spl::StorageFactory::createStorage() function will receive url to base folder
      For example:
      \code
      'ftp://10.10.2.54/some/path'.
      \endcode
    - All other API functions which have URL parameter will receive URL without scheme and host.
      For example:
      \code
      /some/path/folder1/folder2
      \endcode
    - In FileInfoIterator::next() plug-in should provide full URL to target location (with or without scheme + host).
      For example:
      \code
      fileInfo.url = ftp://10.10.2.54/some/path/file1.mkv   // right
      fileInfo.url = /some/path/file1.mkv                   // right
      fileInfo.url = file1.mkv                              // wrong
      \endcode
    - Some of the API functions are called frequently, for example open(), some - based on some (sometimes quite long) timeout, for example, removeDir().
      But MediaServer calls all of them in time, so every function should be implemented correctly.
    - For some storages types, for example FTP, it may be impossible to 'honestly' implement such fuctions as getTotalSpace() and getFreeSpace().
      In this case these functions are allowed to return some sensible constant value. Keep in mind that getTotalSpace() is used mainly in determing "best"​ storage algorithm.
      For example if some storage total space is far smaller than others this storage won't be selected for writing.
      Or if some storage is twice larger than another, it will be written on twice more data. Also total space value is displayed in client for every storage in list.
      getFreeSpace() is used to rotate data on storages. If MediaServer sees that some storage free space becomes lower than predefined limit (5gb by default)
      it will try to remove some old files and folders on this storage.

    \section build_how_to Build how-to
    Use provided CMakeLists.txt project file to generate solution for your favorite build tool or IDE.
    There is also ready to use project for visual studio users in vs2012 folder.
    There are no external dependencies used in this example. All you need to build the library is a relatively modern C++ compiler.

    \section usage Usage
    You should have NX Mediaserver installed. Put built plugin library in mediaserver/bin/plugins (linux) or mediaserver/plugins (windows) folder and restart mediaserver.
    Connect to the server with client. In external storage selection dialog you should be able to see new storage type (FTP).
    Enter valid ftp url and credentials and press Ok. For example,
	\code
	Url:		ftp://10.2.3.87/path/to/storage
	Login:		user1
	Password:	12345678
	\endcode
*/

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

            int p_addRef() { return ++m_count; }

            int p_releaseRef()
            {
                int new_count = --m_count;
                if (new_count <= 0) {
                    delete static_cast<P*>(this);
                }
                return new_count;
            }
        private:
            std::atomic<int> m_count;
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

        virtual unsigned int addRef() override;
        virtual unsigned int releaseRef() override;

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
        std::string         m_localfile;
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

        virtual unsigned int addRef() override;
        virtual unsigned int releaseRef() override;

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
        // and we want to handle it explicitely.
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

        virtual uint64_t STORAGE_METHOD_CALL getFreeSpace(int* ecode) const;
        virtual uint64_t STORAGE_METHOD_CALL getTotalSpace(int* ecode) const;
        virtual int STORAGE_METHOD_CALL getCapabilities() const;

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

        virtual unsigned int addRef() override;
        virtual unsigned int releaseRef() override;

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

        virtual unsigned int addRef() override;
        virtual unsigned int releaseRef() override;
    }; // class FtpStorageFactory
} // namespace Qn

#endif // __FTP_THIRD_PARTY_LIBRARY_H__
