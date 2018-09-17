#include "layout_file_info.h"

#include <exception>
#include <QtCore/QFile>

#include <utils/crypt/crypted_file_stream.h>

namespace {

} // namespace

nx::core::layout::FileInfo nx::core::layout::identifyFile(const QString& fileName)
{
    FileInfo info;

    try
    {
        QString extension = QFileInfo(fileName).suffix().toLower();

        if (extension != "nov" &&  extension != "exe")
            throw std::exception();

        QFile file(fileName);

        if (!file.open(QIODevice::ReadOnly))
            throw std::exception();

        StreamIndex index;
        CryptoInfo crypto;

        if (extension == "exe")
        {
            file.seek(file.size() - 2 * sizeof(qint64));
            quint64 magic;
            file.read((char*)&info.offset, sizeof(qint64));
            file.read((char*)&magic, sizeof(qint64));
            if (magic != kFileMagic)
                throw std::exception();

            file.seek(info.offset);
        }

        if (file.read((char *)&index, sizeof(index)) != sizeof(index))
            throw std::exception();

        info.version = index.version;

        if (index.magic == kIndexCryptedMagic)
        {
            if (file.read((char*)&crypto, sizeof(crypto)) != sizeof(crypto))
                throw std::exception();

            info.isCrypted = true;
            info.passwordHash = crypto.passwordHash;
        }
        else if (index.magic != kIndexMagic)
            throw std::exception();

        info.isValid = true;
    }
    catch(const std::exception&)
    {
    }

    return info;
}

bool nx::core::layout::checkPassword(const QString& password, const FileInfo& fileInfo)
{
    return nx::utils::CryptedFileStream::checkPassword(password, fileInfo.passwordHash);
}
