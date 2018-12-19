#include "crypted_file_stream.h"

#include <stdexcept>
#include <algorithm>
#include <vector>

#include <openssl/evp.h>

using namespace nx::utils::crypto_functions;

namespace nx::utils {

static const Key IV { //< Actually makes little sense because first 8 bytes of IV are overwritten with block index.
    0xf1, 0x8a, 0xdb, 0x71, 0x8b, 0x86, 0xb, 0x7c, 0xf1, 0xa6, 0xb8, 0xff, 0x81, 0x81, 0x64, 0x66,
    0x48, 0xb6, 0x30, 0xfb, 0x3, 0xbc, 0xa2, 0xd, 0x3d, 0xf1, 0xa1, 0xf4, 0xfd, 0xf1, 0xe7, 0xb4
};

CryptedFileStream::CryptedFileStream(const QString& fileName, const QString& password):
    m_mutex(QnMutex::Recursive),
    m_fileName(fileName),
    m_IV(IV),
    m_file(fileName)
{
    m_context = EVP_CIPHER_CTX_new();
    NX_ASSERT(m_context);
    NX_ASSERT(EVP_MD_size(EVP_sha256()) == kKeySize);

    m_mdContext = EVP_MD_CTX_create();
    NX_ASSERT(m_mdContext);

    resetState();
    setPassword(password);
}

CryptedFileStream::CryptedFileStream(const QString& fileName, qint64 position, qint64 size,
    const QString& password)
    : CryptedFileStream(fileName, password)
{
    setEnclosure(position, size);
}

CryptedFileStream::~CryptedFileStream()
{
    close();
    EVP_CIPHER_CTX_free((EVP_CIPHER_CTX*) m_context);
    EVP_MD_CTX_destroy((EVP_MD_CTX*) m_mdContext);
}

void CryptedFileStream::setEnclosure(qint64 position, qint64 size)
{
    NX_ASSERT(m_openMode == NotOpen);
    m_enclosure.position = position;
    m_enclosure.size = size;
    m_enclosure.originalSize = size;
}

void CryptedFileStream::setPassword(const QString& password)
{
    m_passwordKey = adaptPassword(password.toUtf8()); //< Convert to utf8 and adapt to Key.
}

bool CryptedFileStream::open(QIODevice::OpenMode openMode)
{
    // TODO: Add error-checking.
    QnMutexLocker lock(&m_mutex);
    close();

    m_file.setFileName(m_fileName);

    OpenMode fileOpenMode = openMode & (~OpenMode(Text)); //< Clear Text flag from underlying file.
    // Since we may have to read unfinished blocks, we should never open in WriteOnly mode.
    // For Append we need to read header and modify last block.
    if (openMode == WriteOnly || (openMode & Append))
        fileOpenMode = ReadWrite;

    // Ask Misha S. and Sergey I. about why not to use exceptions here - I just don't know.
    auto handleError =
        [this](const QString& str) //< Expanded to several strings for setting breakpoints.
        {
            setErrorString(str);
            return false;
        };

    if (!m_file.open(fileOpenMode))
        return handleError(m_file.errorString());

    m_openMode = openMode;

    m_enclosure.size = m_enclosure.originalSize;

    // Check for probably truncated file or damaged stream.
    if (m_enclosure.size != 0 && (m_enclosure.size - kHeaderSize) % kCryptoBlockSize != 0)
        return handleError(tr("Wrong crypted stream size."));

    if (m_enclosure.isNull() && openMode != WriteOnly) //< Adjust to file size except if WriteOnly.
        m_enclosure.size = m_file.size();

    if (openMode == WriteOnly)
        createHeader();
    else
    {
        if (!readHeader())
            return handleError(tr("Damaged crypted stream header."));
    }

    m_openMode = openMode;
    QIODevice::open(openMode);

    if (openMode & Append)
        seek(m_header.dataSize);
    else
        seek(0);

    return true;
}

void CryptedFileStream::close()
{
    QnMutexLocker lock(&m_mutex);

    if (m_openMode != NotOpen && isWriting())
    {
        dumpCurrentBlock();
        writeHeader();
    }

    m_file.close();
    QIODevice::close();

    resetState();
}

qint64 CryptedFileStream::readData(char* data, qint64 maxSize)
{
    // TODO: Add error-checking & read data counting.
    QnMutexLocker lock(&m_mutex);
    maxSize = std::min(maxSize, m_header.dataSize - m_position.position());
    qint64 toRead = maxSize;

    const qint64 head = std::min(maxSize, kCryptoBlockSize - m_position.positionInBlock);
    readFromBlock(data, head);
    toRead -= head;

    while (toRead > kCryptoBlockSize)
    {
        advanceBlock();
        readFromBlock(data + (maxSize - toRead), kCryptoBlockSize);
        toRead -= kCryptoBlockSize;
    }

    if (toRead > 0)
    {
        advanceBlock();
        readFromBlock(data + (maxSize - toRead), toRead);
    }

    return maxSize;
}

qint64 CryptedFileStream::writeData(const char* data, qint64 maxSize)
{
    // TODO: Add error-checking & write data counting.
    QnMutexLocker lock(&m_mutex);
    qint64 toWrite = maxSize;

    const qint64 head = std::min(maxSize, kCryptoBlockSize - m_position.positionInBlock);
    writeToBlock(data, head);
    toWrite -= head;

    while (toWrite > kCryptoBlockSize)
    {
        advanceBlock();
        writeToBlock(data + (maxSize - toWrite), kCryptoBlockSize);
        toWrite -= kCryptoBlockSize;
    }

    if (toWrite > 0)
    {
        advanceBlock();
        writeToBlock(data + (maxSize - toWrite), toWrite);
    }

    return maxSize;
}

bool CryptedFileStream::seek(qint64 offset)
{
    QnMutexLocker lock(&m_mutex);
    // Qt docs say: "When subclassing QIODevice, you must call QIODevice::seek() at the start
    // of your function to ensure integrity with QIODevice's built-in buffer."
    QIODevice::seek(offset);

    if (!isWriting() && (offset > m_header.dataSize))
        return false;

    dumpCurrentBlock();
    m_position.setPosition(offset);
    loadCurrentBlock();

    return true;
}

qint64 CryptedFileStream::pos() const
{
    QnMutexLocker lock(&m_mutex);
    return QIODevice::pos();
}

qint64 CryptedFileStream::size() const
{
    QnMutexLocker lock(&m_mutex);
    return m_header.dataSize;
}

qint64 CryptedFileStream::grossSize() const
{
    QnMutexLocker lock(&m_mutex);
    // Calculating block number including "tail" block.
    return kHeaderSize
        + ((m_header.dataSize + kCryptoBlockSize - 1) / kCryptoBlockSize) * kCryptoBlockSize;
}

void CryptedFileStream::resetState()
{
    m_blockDirty = false;
    m_position = Position();
    m_header = Header();
    memset(m_currentPlainBlock, 0, kCryptoBlockSize);
    m_openMode = NotOpen;
}

void CryptedFileStream::readFromBlock(char* data, qint64 count)
{
    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(data, m_currentPlainBlock + m_position.positionInBlock, count);
    m_position.positionInBlock += count;
}

void CryptedFileStream::writeToBlock(const char* data, qint64 count)
{
    if (count == 0) //< Nothing to write.
        return;

    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(m_currentPlainBlock + m_position.positionInBlock, data, count);

    m_blockDirty = true;
    m_position.positionInBlock += count;

    m_header.dataSize = std::max(m_header.dataSize, m_position.position());
}

void CryptedFileStream::advanceBlock()
{
    dumpCurrentBlock();
    m_position.blockIndex ++;
    m_position.positionInBlock = 0;
    loadCurrentBlock();
}

void CryptedFileStream::dumpCurrentBlock()
{
    if (!m_blockDirty || !isWriting())
        return;

    cryptBlock();
    m_file.seek(m_enclosure.position + kHeaderSize + kCryptoBlockSize * m_position.blockIndex);
    m_file.write(m_currentCryptedBlock, kCryptoBlockSize);
    m_blockDirty = false;
}

void CryptedFileStream::loadCurrentBlock()
{
    const bool shouldRead = isWriting()
        // Will load if there is any data in the block.
        ? m_position.blockIndex * kCryptoBlockSize < m_header.dataSize
        // Will load if entire block fits into enclosure.
        : ((m_position.blockIndex + 1) * kCryptoBlockSize <= m_enclosure.size);

    if (shouldRead)
    {
        m_file.seek(m_enclosure.position + kHeaderSize + kCryptoBlockSize * m_position.blockIndex);
        m_file.read(m_currentCryptedBlock, kCryptoBlockSize);
        decryptBlock();
    }
}

void CryptedFileStream::createHeader()
{
    m_header = Header();

    m_header.salt = getRandomSalt();
    m_key = getKeyHash(xorKeys(m_passwordKey, m_header.salt));
    m_header.keyHash = getKeyHash(m_key);

    writeHeader();
}

bool CryptedFileStream::readHeader()
{
    if (m_enclosure.size < kHeaderSize)
        return false;

    m_file.seek(m_enclosure.position);
    if (m_file.read((char *)&m_header, sizeof(m_header)) != sizeof(m_header))
        return false;

    if (m_header.minReadVersion > kCryptoStreamVersion) //< The file is too new.
        return false;

    m_key = getKeyHash(xorKeys(m_passwordKey, m_header.salt));

    return getKeyHash(m_key) == m_header.keyHash;
}

void CryptedFileStream::writeHeader()
{
    m_file.seek(m_enclosure.position);
    m_file.write((char *)&m_header, sizeof(m_header));
    // Fill rest with zeroes.
    static char zeroes[kHeaderSize - sizeof(m_header)];
    memset(zeroes, 0, kHeaderSize - sizeof(m_header));
    m_file.write(zeroes, kHeaderSize - sizeof(m_header));
}

// Actual encrypring & decrypting.
void CryptedFileStream::cryptBlock()
{
    // Create IV from block index.
    auto result = EVP_DigestInit_ex((EVP_MD_CTX*) m_mdContext, EVP_sha256(), nullptr);
    NX_ASSERT(result);
    result = EVP_DigestUpdate((EVP_MD_CTX*) m_mdContext, &m_position.blockIndex, sizeof(qint64));
    NX_ASSERT(result);
    unsigned int mdLen;
    result = EVP_DigestFinal_ex((EVP_MD_CTX*) m_mdContext, m_IV.data(), &mdLen);
    NX_ASSERT(result && mdLen <= m_IV.size());

    // Encrypt block.
    result = EVP_EncryptInit_ex((EVP_CIPHER_CTX*) m_context , EVP_aes_256_cbc(),
        nullptr, m_key.data(), m_IV.data());
    EVP_CIPHER_CTX_set_padding((EVP_CIPHER_CTX*) m_context, 0);
    NX_ASSERT(result);

    int cryptlen;
    result = EVP_EncryptUpdate((EVP_CIPHER_CTX*) m_context, (unsigned char *) m_currentCryptedBlock, &cryptlen,
        (unsigned char *) m_currentPlainBlock, kCryptoBlockSize);
    NX_ASSERT(result);

    unsigned char dummy[32]; //< Actually 16 is enough for AES.
    result = EVP_EncryptFinal_ex((EVP_CIPHER_CTX*) m_context, dummy, &cryptlen);
    NX_ASSERT(result && (cryptlen == 0)); //< No extra bytes should be written to crypted buffer.
}

void CryptedFileStream::decryptBlock()
{
    // Create IV from block index.
    auto result = EVP_DigestInit_ex((EVP_MD_CTX*) m_mdContext, EVP_sha256(), nullptr);
    NX_ASSERT(result);
    result = EVP_DigestUpdate((EVP_MD_CTX*) m_mdContext, &m_position.blockIndex, sizeof(qint64));
    NX_ASSERT(result);
    unsigned int mdLen;
    result = EVP_DigestFinal_ex((EVP_MD_CTX*) m_mdContext, m_IV.data(), &mdLen);
    NX_ASSERT(result && mdLen <= m_IV.size());

    // Decrypt block.
    result = EVP_DecryptInit_ex((EVP_CIPHER_CTX*) m_context , EVP_aes_256_cbc(),
        nullptr, m_key.data(), m_IV.data());
    EVP_CIPHER_CTX_set_padding((EVP_CIPHER_CTX*) m_context, 0);
    NX_ASSERT(result);

    int cryptLen;
    result = EVP_DecryptUpdate((EVP_CIPHER_CTX*) m_context, (unsigned char *) m_currentPlainBlock, &cryptLen,
        (unsigned char *) m_currentCryptedBlock, kCryptoBlockSize);
    NX_ASSERT(result);

    unsigned char dummy[32];
    result = EVP_DecryptFinal_ex((EVP_CIPHER_CTX*) m_context, dummy, &cryptLen);
    NX_ASSERT(result && (cryptLen == 0)); //< No extra bytes should be written to decrypted buffer.
}

} // namespace nx::utils
