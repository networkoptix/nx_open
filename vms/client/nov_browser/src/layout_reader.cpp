#include "layout_reader.h"

#include <stdexcept>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <utils/common/util.h>
#include <core/resource/avi/filetypesupport.h>
#include <utils/crypt/crypted_file_stream.h>

using namespace nx::core::layout;
using std::runtime_error;

int getTailSize(bool isExe)
{
    return isExe ? sizeof(qint64) * 2 : 0;
}

bool shouldCrypt(const QString& streamName)
{
    return FileTypeSupport::isMovieFileExt(streamName)
        || FileTypeSupport::isImageFileExt(streamName);
}

void readHeader(LayoutStructure& structure)
{
    try
    {
        const QString extension = QFileInfo(structure.fileName).suffix().toLower();

        if (!(extension == "nov" || extension == "exe"))
            throw runtime_error("Bad extension");

        QFile file(structure.fileName);

        if (!file.open(QIODevice::ReadOnly))
            throw runtime_error("Cannot open file");

        StreamIndex index;
        CryptoInfo crypto;

        // Check EXE enclosure.
        if (extension == "exe")
        {
            structure.isExe = true;

            file.seek(file.size() - 2 * sizeof(qint64));
            quint64 magic;
            quint64 offset;
            file.read((char*)&offset, sizeof(qint64));
            file.read((char*)&magic, sizeof(qint64));
            if (magic != kFileMagic)
                throw runtime_error("No magic in EXE file");

            structure.info.offset = offset;
            file.seek(structure.info.offset);
        }

        // Read index.
        if (file.read((char *)&index, sizeof(index)) != sizeof(index))
            throw runtime_error("Index header is not complete");

        structure.info.version = index.version;

        if (index.magic == kIndexCryptedMagic)
        {
            // Read ercryption info.
            if (file.read((char*)&crypto, sizeof(crypto)) != sizeof(crypto))
                throw std::runtime_error("Index header is not complete");

            structure.info.isCrypted = true;
            structure.info.passwordSalt = crypto.passwordSalt;
            structure.info.passwordHash = crypto.passwordHash;
        }
        else if (index.magic != kIndexMagic)
            throw runtime_error("Index magic is damaged");

        // Decompose Index.
        if (index.entryCount < 1)
            throw runtime_error("Empty index - no streams");

        // Identify streams.
        for (qint64 i = 0; i < index.entryCount; i++)
        {
            if (!file.seek(index.entries[i].offset + structure.info.offset))
                throw runtime_error("Error seeking stream in file");;

            char tmpBuffer[1024]; // buffer size is max file len
            int readBytes = file.read(tmpBuffer, sizeof(tmpBuffer));
            QString streamName = QString::fromUtf8(QByteArray(tmpBuffer, readBytes));
            if (qt4Hash(streamName) != index.entries[i].fileNameCrc)
                throw runtime_error("Index entry CRC does not match stream name");

            LayoutStreamInfo stream;
            stream.name = streamName;
            stream.position = index.entries[i].offset + structure.info.offset
                + streamName.toUtf8().length() + 1;
            if (i < index.entryCount - 1)
            {
                stream.grossSize = index.entries[i + 1].offset + structure.info.offset - stream.position;
            }
            else
            {
                qint64 endPos = file.size() - getTailSize(structure.isExe);
                stream.grossSize = endPos - stream.position;
            }

            stream.isCrypted = structure.info.isCrypted && shouldCrypt(streamName);
            if (stream.isCrypted)
            {
                nx::utils::CryptedFileStream cryptoStream(structure.fileName);
                cryptoStream.setEnclosure(stream.position, stream.grossSize);
                cryptoStream.open(QIODevice::ReadOnly); //< Fails without password but reads header.
                stream.size = cryptoStream.size();
            }
            else
            {
                stream.size = stream.grossSize;
            }

            structure.streams.push_back(stream);
        }

        structure.info.isValid = true;
    }
    catch (const std::exception& e)
    {
        structure.errorText = e.what();
    }
}



bool readLayoutStructure(const QString& fileName, LayoutStructure& structure)
{
    structure.fileName = fileName;
    readHeader(structure);
    return structure.info.isValid;
}
