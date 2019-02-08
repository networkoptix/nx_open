#include "layout_file_info.h"

#include <exception>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

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

    const QString extension = QFileInfo(fileName).suffix().toLower();

    if (!(extension == "nov" || extension == "exe"
        || (allowTemp && (fileName.endsWith(".exe.tmp") || fileName.endsWith(".nov.tmp")))))
    {
        return info;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly))
        return info;

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
            return info;

        info.offset = offset;
        file.seek(info.offset);
    }

    if (file.read((char *)&index, sizeof(index)) != sizeof(index))
        return info;

    info.version = index.version;

    if (index.magic == kIndexCryptedMagic)
    {
        if (file.read((char*)&crypto, sizeof(crypto)) != sizeof(crypto))
            return info;

        info.isCrypted = true;
        info.passwordSalt = crypto.passwordSalt;
        info.passwordHash = crypto.passwordHash;
    }
    else if (index.magic != kIndexMagic)
        return info;

    info.isValid = true;

    return info;
}

bool checkPassword(const QString& password, const FileInfo& fileInfo)
{
    return nx::utils::crypto_functions::checkSaltedPassword(password,
        fileInfo.passwordSalt, fileInfo.passwordHash);
}

} // namespace nx::core::layout
