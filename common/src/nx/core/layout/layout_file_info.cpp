#include "layout_file_info.h"

#include <exception>
#include <QtCore/QFile>

#include <utils/crypt/crypted_file_stream.h>

namespace nx::core::layout {

bool isLayoutExtension(const QString& fileName)
{
    const QString extension = QFileInfo(fileName).suffix().toLower();
    return extension == "nov" || extension == "exe";
}

FileInfo identifyFile(const QString& fileName, bool allowTemp)
{
    FileInfo info;

    try
    {
        const QString extension = QFileInfo(fileName).suffix().toLower();

        if (!(extension == "nov" || extension == "exe"
            || (allowTemp && (fileName.endsWith(".exe.tmp") || fileName.endsWith(".nov.tmp")))))
        {
            throw std::exception();
        }

        QFile file(fileName);

        if (!file.open(QIODevice::ReadOnly))
            throw std::exception();

        StreamIndex index;
        CryptoInfo crypto;

        if (extension == "exe")
        {
            file.seek(file.size() - 2 * sizeof(qint64));
            quint64 magic;
            quint64 offset;
            file.read((char*)&offset, sizeof(qint64));
            file.read((char*)&magic, sizeof(qint64));
            if (magic != kFileMagic)
                throw std::exception();

            info.offset = offset;
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
            info.passwordSalt = crypto.passwordSalt;
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

bool checkPassword(const QString& password, const FileInfo& fileInfo)
{
    return nx::utils::crypto_functions::checkSaltedPassword(password,
        fileInfo.passwordSalt, fileInfo.passwordHash);
}

} // namespace nx::core::layout
