#include <cstdio>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <cstring>
#include <plugins/storage/third_party/third_party_storage.h>
#include "library.h"

#define CPREF ">> "

namespace client
{
    class StorageManager
    {
        typedef std::shared_ptr<
            Qn::Storage
        > StoragePtrType;

        typedef std::shared_ptr<
            Qn::FileInfoIterator
        > FileInfoIteratorPtrType;

        typedef std::shared_ptr<
            Qn::IODevice
        > IODevicePtrType;

    public: // ctor(s), dtor
        StorageManager(const char* s)
            : m_lib(s)
        {
            if (!m_lib.ok())
                throw std::runtime_error("Load library failed");

            m_csf = static_cast<create_qn_storage_factory_function>(
                m_lib.symbol("create_qn_storage_factory")
            );
    
            m_emf = static_cast<qn_storage_error_message_function>(
                m_lib.symbol("qn_storage_error_message")
            );

            if (m_csf == nullptr || m_emf == nullptr)
                throw std::runtime_error("Couldn't load library functions");
        }
    public: // manage storage
        int openStorage(const char* url)
        {
            Qn::StorageFactory* sfRaw = m_csf();
            if (sfRaw == nullptr)
                return Qn::error::UnknownError;

            std::shared_ptr<Qn::StorageFactory> sf = 
                std::shared_ptr<Qn::StorageFactory>(
                    sfRaw, 
                    [](Qn::StorageFactory* p)
                    {
                        p->releaseRef();
                    }
                );

            int ecode = Qn::error::NoError;
            Qn::Storage* spRaw = sf->createStorage(url, &ecode);

            if (ecode != Qn::error::NoError)
            {
                if (spRaw != nullptr)
                    spRaw->releaseRef();
                return ecode;
            }

            if (spRaw == nullptr)
                return Qn::error::UnknownError;

            std::shared_ptr<Qn::Storage> sp =
                std::shared_ptr<Qn::Storage>(
                    spRaw,
                    [](Qn::Storage* p)
                    {
                        p->releaseRef();
                    }
                );

            void* queryResult = nullptr;
            if (queryResult = sp->queryInterface(Qn::IID_Storage))
            {
                m_stor.reset(
                    static_cast<Qn::Storage*>(queryResult),
                    [](Qn::Storage* p)
                    {
                        p->releaseRef();
                    }
                );
            }
            else
            {
                throw std::logic_error("Unknown storage interface version");
            }
            
            return ecode;
        }

        void printAvailable() const 
        {
            printf(
                CPREF"\tStorage available:\t%s\n" 
                CPREF,
                m_stor->isAvailable() ? "Yes" : "No"
            );
        }

        void printCapabilities() const
        {
            int cap = m_stor->getCapabilities();
            printf(
                CPREF"Storage capabilities: \n"
                CPREF"\tList:\t\t%s\n" 
                CPREF"\tRemove:\t\t%s\n" 
                CPREF"\tRead:\t\t%s\n" 
                CPREF"\tWrite:\t\t%s\n" 
                CPREF,
                cap & Qn::cap::ListFile ? "Yes" : "No",
                cap & Qn::cap::RemoveFile ? "Yes" : "No",
                cap & Qn::cap::ReadFile ? "Yes" : "No",
                cap & Qn::cap::WriteFile ? "Yes" : "No"
            );
        }

        void printFreeSpace()
        {
            int ecode;
            uint64_t freeSpace = m_stor->getFreeSpace(&ecode);

            if (ecode != Qn::error::NoError)
                printf(
                    CPREF"\terror while retrieving storage free space info: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
            else
                printf(
                    CPREF"\t%ul\n"
                    CPREF,
                    freeSpace
                );
        }

        void printTotalSpace()
        {
            int ecode;
            uint64_t totalSpace = m_stor->getTotalSpace(&ecode);

            if (ecode != Qn::error::NoError)
                printf(
                    CPREF"\terror while retrieving storage total space info: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
            else
                printf(
                    CPREF"\t%ul\n"
                    CPREF,
                    totalSpace
                );
        }

        void printRmFile(const char* cmd)
        {
            if (cmd[2] != ' ' || cmd[3] == '\0')
            {
                printf(
                    CPREF"\t error: 'rm' requires file uri\n"
                    CPREF
                );
                return;
            }
            else
            {
                int ecode;
                m_stor->removeFile(&cmd[3], &ecode);

                if (ecode != Qn::error::NoError)
                    printf(
                        CPREF"\terror while removing file %s from storage: %s\n"
                        CPREF,
                        &cmd[3],
                        m_emf(ecode)
                    );
                else
                    printf(
                        CPREF"\t%s successfully removed\n"
                        CPREF,
                        &cmd[3]
                    );
            }
        }

        void printRmDir(const char* cmd)
        {
            if (cmd[3] != ' ' || cmd[4] == '\0')
            {
                printf(
                    CPREF"\t error: 'rmd' requires dir uri\n"
                    CPREF
                );
                return;
            }
            else
            {
                int ecode;
                m_stor->removeDir(&cmd[4], &ecode);

                if (ecode != Qn::error::NoError)
                    printf(
                        CPREF"\terror while removing dir %s from storage: %s\n"
                        CPREF,
                        &cmd[4],
                        m_emf(ecode)
                    );
                else
                    printf(
                        CPREF"\t%s successfully removed\n"
                        CPREF,
                        &cmd[4]
                    );
            }
        }

        void printRenameFile(char* cmd)
        {
            const char* cmd_name;
            const char* from;
            const char* to;

            cmd_name = strtok(cmd, " ");
            from = strtok(NULL, " ");
            to = strtok(NULL, " ");

            if (from == nullptr || to == nullptr)
            {
                printf(
                    CPREF"\t error: 'rnm' requires two file uris\n"
                    CPREF
                );
                return;
            }

            int ecode;
            m_stor->renameFile(from, to, &ecode);

            if (ecode != Qn::error::NoError)
                printf(
                    CPREF"\terror while renaming file %s: %s\n"
                    CPREF,
                    from,
                    m_emf(ecode)
                );
            else
                printf(
                    CPREF"\t%s successfully renamed\n"
                    CPREF,
                    from
                );
        }

        void printGetFileInfoIterator(char* cmd)
        {
            const char* cmd_name;
            const char* dir;

            cmd_name = strtok(cmd, " ");
            dir = strtok(NULL, " ");

            if (dir == nullptr)
            {
                printf(
                    CPREF"\terror: 'iterd' requires directory uri\n"
                    CPREF
                );
                return;
            }

            int ecode;
            Qn::FileInfoIterator* fitRaw = m_stor->getFileIterator(dir, &ecode);
            if (fitRaw == nullptr)
            {
                printf(
                    CPREF"\terror: FileInfoIterator is NULL\n"
                    CPREF
                );
                return;
            }
            else if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror while trying to get FileInfoIterator: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
                fitRaw->releaseRef();
                return;
            }

            void *queryResult = fitRaw->queryInterface(Qn::IID_FileInfoIterator);
            if (queryResult == nullptr)
            {
                printf(
                    CPREF"\terror: FileInfoIterator::queryInterface failed\n"
                    CPREF
                );
                fitRaw->releaseRef();
                return;
            }
            else
            {
                m_fit.reset(
                    static_cast<Qn::FileInfoIterator*>(queryResult),
                    [](Qn::FileInfoIterator *p)
                    {
                        p->releaseRef();
                    }
                );
                printf(
                    CPREF"\tFileInfoIterator for %s successfully acquired! Now use 'nextfi' command to iterate\n"
                    CPREF,
                    dir
                );
                fitRaw->releaseRef();
            }
        }

        void printNextFileInfo()
        {
            if (!m_fit)
            {
                printf(
                    CPREF"\terror: FileInfoIterator is NULL\n"
                    CPREF
                );
                return;
            }

            int ecode;
            Qn::FileInfo *fi = m_fit->next(&ecode);

            if (fi == nullptr)
            {
                printf(
                    CPREF"\tnext FileInfo is NULL. Seems like that we've reached the end of the list\n"
                    CPREF
                );
                return;
            }
            else if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror while trying to get next FileInfo: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
                return;
            }

            printf(
                CPREF"\tFileInfoIterator::next is successfull\n"
                CPREF"\tfile uri: \t%s\n"
                CPREF"\tfile size: \t%lu\n"
                CPREF"\tfile type: \t%s\n"
                CPREF, 
                fi->url, 
                (long unsigned)fi->size, 
                fi->type == Qn::isFile ? "File" : "Dir"
            );
        }

        void printFileSize(char* cmd)
        {
            const char* cmd_name;
            const char* fname;

            cmd_name = strtok(cmd, " ");
            fname = strtok(NULL, " ");

            if (fname == nullptr)
            {
                printf(
                    CPREF"\terror: 'fsize' requires file uri\n"
                    CPREF
                );
                return;
            }

            int ecode;
            unsigned long fsize = (unsigned long)m_stor->fileSize(fname, &ecode);

            if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror while trying to get file size for %s: %s\n"
                    CPREF,
                    fname,
                    m_emf(ecode)
                );
                return;
            }

            printf(
                CPREF"\t%s size: %lu\n"
                CPREF,
                fname,
                fsize
            );
        }

        void printFileExists(char* cmd)
        {
            const char* cmd_name;
            const char* fname;

            cmd_name = strtok(cmd, " ");
            fname = strtok(NULL, " ");

            if (fname == nullptr)
            {
                printf(
                    CPREF"\terror: 'fexists' requires file uri\n"
                    CPREF
                );
                return;
            }

            int ecode;
            int result = (unsigned long)m_stor->fileExists(fname, &ecode);

            if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror while trying to get file existance info for %s: %s\n"
                    CPREF,
                    fname,
                    m_emf(ecode)
                );
                return;
            }

            printf(
                CPREF"\t%s exists: %s\n"
                CPREF,
                fname,
                result == 0 ? "No" : "Yes"
            );
        }

        void printDirExists(char* cmd)
        {
            const char* cmd_name;
            const char* dname;

            cmd_name = strtok(cmd, " ");
            dname = strtok(NULL, " ");

            if (dname == nullptr)
            {
                printf(
                    CPREF"\terror: 'dexists' requires dir uri\n"
                    CPREF
                );
                return;
            }

            int ecode;
            int result = (unsigned long)m_stor->dirExists(dname, &ecode);

            if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror while trying to get dir existance info for %s: %s\n"
                    CPREF,
                    dname,
                    m_emf(ecode)
                );
                return;
            }

            printf(
                CPREF"\t%s exists: %s\n"
                CPREF,
                dname,
                result == 0 ? "No" : "Yes"
            );
        }

        void printFileOpen(char* cmd)
        {
            const char* cmd_name;
            const char* file;
            const char* flags;

            cmd_name = strtok(cmd, " ");
            file = strtok(NULL, " ");
            flags = strtok(NULL, " ");

            if (file == nullptr || flags == nullptr)
            {
                printf(
                    CPREF"\terror: 'fopen' requires file uri and open flags\n"
                    CPREF
                );
                return;
            }

            int ecode;
            int ioFlags = 0;
            if (flags[0] == 'r')
            {
                ioFlags |= Qn::io::ReadOnly;
                if (strlen(flags) == 2 && flags[1] == 'w')
                    ioFlags |= Qn::io::WriteOnly;
            }
            else if (flags[0] == 'w')
                ioFlags |= Qn::io::WriteOnly;

            Qn::IODevice* ioRaw = m_stor->open(file, ioFlags, &ecode);
            if (ioRaw == nullptr)
            {
                printf(
                    CPREF"\terror: IODevice is NULL\n"
                    CPREF
                );
                return;
            }
            else if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror while trying to get IODevice: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
                ioRaw->releaseRef();
                return;
            }

            void *queryResult = ioRaw->queryInterface(Qn::IID_IODevice);
            if (queryResult == nullptr)
            {
                printf(
                    CPREF"\terror: IODevice::queryInterface failed\n"
                    CPREF
                );
                ioRaw->releaseRef();
                return;
            }
            else
            {
                m_io.reset(
                    static_cast<Qn::IODevice*>(queryResult),
                    [](Qn::IODevice *p)
                    {
                        p->releaseRef();
                    }
                );
                printf(
                    CPREF"\tIODevice for %s successfully acquired! Now use 'fwrite/fread/fseek/fclose' commands manipulate this device\n"
                    CPREF,
                    file
                );
                ioRaw->releaseRef();
            }
        }

        // if same local file name specified for several sequential reads,
        // data is appended to this file
        void printFileRead(char* cmd)
        {
            const char* cmd_name;
            const char* target;
            const char* count;

            cmd_name = strtok(cmd, " ");
            target = strtok(NULL, " ");
            count = strtok(NULL, " ");

            if (target == nullptr || count == nullptr)
            {
                printf(
                    CPREF"\terror: 'fread' requires local file name and bytes count\n"
                    CPREF
                );
                return;
            }

            if (!m_io)
            {
                printf(
                    CPREF"\terror: current IODevice is NULL. try to open one with 'fopen' first\n"
                    CPREF
                );
                return;
            }

            int ecode;
            FILE *t = NULL;
            uint32_t bCount = strtoul(count, NULL, 10);
            if (bCount == 0)
            {
                printf(
                    CPREF"\terror: invalid read count argument value\n"
                    CPREF
                );
                goto end;
            }

            char *buf = (char *)malloc(bCount);
            if (!buf)
                goto end;
            
            m_io->read(buf, bCount, &ecode);

            if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
                goto end;
            }

            t = fopen(target, "ab");
            if (t == NULL)
            {
                printf(
                    CPREF"\terror: couldn't open local target file\n"
                    CPREF
                );
                goto end;
            }

            size_t written = fwrite(buf, 1, bCount, t);
            free(buf);
            if (written < bCount)
            {
                if (ferror(t))
                {
                    printf(
                        CPREF"\terror: error writing to local target file\n"
                        CPREF
                    );
                    goto end;
                }
            }

            printf(
                CPREF"\tsuccessfully read %u bytes. check local file '%s' for results\n"
                CPREF,
                written,
                target
            );
            fclose(t);
            return;

        end:
            free(buf);
            if (t != NULL)
                fclose(t);
        }

        void printFileClose()
        {
            if (!m_io)
            {
                printf(
                    CPREF"\terror: IODevice is NULL. Have you opened one?\n"
                    CPREF
                );
                return;
            }

            m_io.reset();
            printf(
                CPREF"\tcurrent IODevice has been successfully closed\n"
                CPREF
            );
        }

        void printFileSeek(char* cmd)
        {
            const char *cmd_name;
            const char *pos;

            cmd_name = strtok(cmd, " ");
            pos = strtok(NULL, " ");

            if (pos == nullptr)
            {
                printf(
                    CPREF"\terror: 'fseek' requires 'pos' parameter\n"
                    CPREF
                );
                return;
            }

            uint64_t seekPos = strtoul(pos, NULL, 10);
            if (seekPos == 0)
            {
                printf(
                    CPREF"\terror: 'pos' argument is invalid\n"
                    CPREF
                );
                return;
            }

            if (!m_io)
            {
                printf(
                    CPREF"\terror: current IODevice is NULL. try to open one with 'fopen' first\n"
                    CPREF
                );
                return;
            }

            int ecode;
            if(m_io->seek(seekPos, &ecode) == 0)
            {
                printf(
                    CPREF"\terror: seek error\n"
                    CPREF
                );
                return;
            }
            printf(
                CPREF"\tseek successfull\n"
                CPREF
            );
        }

        void printFileWrite(char* cmd)
        {
            const char* cmd_name;
            const char* src;

            cmd_name = strtok(cmd, " ");
            src = strtok(NULL, " ");

            if (src == nullptr)
            {
                printf(
                    CPREF"\terror: 'fwrite' requires local file name\n"
                    CPREF
                );
                return;
            }

            if (!m_io)
            {
                printf(
                    CPREF"\terror: current IODevice is NULL. try to open one with 'fopen' first\n"
                    CPREF
                );
                return;
            }

            int ecode;
            FILE *f = fopen(src, "rb");
            char *buf = NULL;
            if (f == NULL)
            {
                printf(
                    CPREF"\terror: local file %s open failed\n"
                    CPREF,
                    src
                );
                return;
            }

            long bCount = 0;
            char cbuf[1];
            while (1)
            {
                fread(cbuf, 1, 1, f);
                if (feof(f) == 0)
                    ++bCount;
                else
                    break;
            }

            if (fseek(f, SEEK_SET, SEEK_SET) != 0)
            {
                printf(
                    CPREF"\terror: local file %s seek failed\n"
                    CPREF,
                    src
                );
                goto end;
            }

            buf = (char *)malloc(bCount);
            if (!buf)
                goto end;
            fread(buf, 1, bCount, f);
            fclose(f);
            f = NULL;
            
            m_io->write(buf, bCount, &ecode);

            if (ecode != Qn::error::NoError)
            {
                printf(
                    CPREF"\terror: %s\n"
                    CPREF,
                    m_emf(ecode)
                );
                goto end;
            }

            printf(
                CPREF"\tsuccessfully written %u bytes to the current IODevice\n"
                CPREF,
                bCount
            );
            free(buf);
            return;

        end:
            free(buf);
            if (f != NULL)
                fclose(f);
        }

        const char* strError(int ecode) const
        {
            return m_emf(ecode);
        }

    private:
        sys::library                        m_lib;
        create_qn_storage_factory_function  m_csf;
        qn_storage_error_message_function   m_emf;
        StoragePtrType                      m_stor;
        FileInfoIteratorPtrType             m_fit;
        IODevicePtrType                     m_io;
    };

    void printHelp()
    {
        printf(
            CPREF"Commands:\n"
            CPREF"\tavail\t\t check if storage available\n"
            CPREF"\tcap\t\t print storage capabilities\n"
            CPREF"\tfrees\t\t get storage free space\n"
            CPREF"\ttotals\t\t get storage total space\n"
            CPREF"\trm file\t\t remove file from storage\n"
            CPREF"\trmd dir\t\t remove dir from storage\n"
            CPREF"\trnm f t\t\t rename file from 'f' to 't'\n"
            CPREF"\titerd dir\t get file info iterator for dir\n"
            CPREF"\tnextfi\t\t get next file info entry from the current iterator\n"
            CPREF"\tfsize f\t\t get size of file 'f'\n"
            CPREF"\tfexists f\t does file 'f' exist\n"
            CPREF"\tdexists d\t does dir 'd' exist\n"
            CPREF"\tfopen f flags\t opens IODevice for 'f' for read/write/seek operations. flags are 'r', 'w' or 'rw'\n"
            CPREF"\tfread trg count\t reads 'count' bytes from the previously opened with fopen IODevice and appends them to the local file named 'trg'\n"
            CPREF"\tfwrite src\t writes local 'src' file contents to previously opened with fopen IODevice\n"
            CPREF"\tfclose\t\t closes currently opened IODevice\n"
            CPREF"\tfseek pos\t seeks 'pos' position in currently opened IODevice\n"
            CPREF"\t\n"
            CPREF"\thelp\t\t show this help\n"
            CPREF"\tquit\t\t quit the program\n"
            CPREF
        );
    }
}


int main(int argc, const char* argv[])
{
    try
    {
        if (argc < 3)
        {
            printf(
                "Usage: test_client library URL\n"
                "\tlibrary - full path to shared library\n"
                "\tURL format: storage://[username:password@]host[:port]\n"
                "\tIf no username and password specified, anonymous is assumed\n"
                "\tIf no port specified, default is assumed (21 for FTP, for example)\n"
            );
            return -1;
        }

        client::StorageManager storMan(argv[1]);
        int ecode;

        if ((ecode = storMan.openStorage(argv[2])) != Qn::error::NoError)
        {
            printf("Create storage error: %s\n", storMan.strError(ecode));
            return 1;
        }
        
        printf("\nWelcome to storage management utility!\n"
                    "Successfully connected to %s\n"
                    "Type 'help' to start\n",
                    argv[2]);

        char buf[256];
        // main loop
        while (1)
        {
            printf("\n"CPREF);
            std::fgets(buf, 256, stdin);
            buf[std::strcspn(buf, "\r\n")] = '\0';

            if (std::strcmp(buf, "help") == 0)
                client::printHelp();
            else if (std::strcmp(buf, "quit") == 0)
                return 0;
            else if (std::strcmp(buf, "avail") == 0)
                storMan.printAvailable();
            else if (std::strcmp(buf, "cap") == 0)
                storMan.printCapabilities();
            else if (std::strcmp(buf, "frees") == 0)
                storMan.printFreeSpace();
            else if (std::strcmp(buf, "totals") == 0)
                storMan.printTotalSpace();
            else if (std::strncmp(buf, "fsize", 5) == 0)
                storMan.printFileSize(buf);
            else if (std::strcmp(buf, "nextfi") == 0)
                storMan.printNextFileInfo();
            else if (std::strncmp(buf, "iterd", 5) == 0)
                storMan.printGetFileInfoIterator(buf);
            else if (std::strncmp(buf, "fexists", 7) == 0)
                storMan.printFileExists(buf);
            else if (std::strncmp(buf, "fopen", 5) == 0)
                storMan.printFileOpen(buf);
            else if (std::strncmp(buf, "fclose", 6) == 0)
                storMan.printFileClose();
            else if (std::strncmp(buf, "fread", 5) == 0)
                storMan.printFileRead(buf);
            else if (std::strncmp(buf, "fwrite", 6) == 0)
                storMan.printFileWrite(buf);
            else if (std::strncmp(buf, "fseek", 5) == 0)
                storMan.printFileSeek(buf);
            else if (std::strncmp(buf, "dexists", 7) == 0)
                storMan.printDirExists(buf);
            else if (std::strncmp(buf, "rnm", 3) == 0)
                storMan.printRenameFile(buf);
            else if (std::strncmp(buf, "rmd", 3) == 0)
                storMan.printRmDir(buf);
            else if (std::strncmp(buf, "rm", 2) == 0)
                storMan.printRmFile(buf);
            else if (std::strlen(buf) == 0)
                printf(CPREF);
            else
                printf(CPREF"unknown command. Try 'help'");
        }
    }
    catch (const std::exception& e)
    {
        printf("Fatal: %s\n", e.what());
        return -1;
    }
}