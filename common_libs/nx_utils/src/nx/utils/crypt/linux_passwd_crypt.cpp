/**********************************************************
* 9 jun 2015
* a.kolesnikov
***********************************************************/

#include "linux_passwd_crypt.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#endif

#include <QCryptographicHash>
#include <QtCore/QFile>

#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>


namespace 
{
    const int SHUFFLE_INDEXES[] =
    { 63, 62, 20, 41, 40, 61, 19, 18, 39, 60, 59, 17, 38, 37, 58, 16, 15, 36, 57, 56, 14, 35,
          34, 55, 13, 12, 33, 54, 53, 11, 32, 31, 52, 10,  9, 30, 51, 50,  8, 29, 28, 49,  7,
          6,  27, 48, 47,  5, 26, 25, 46,  4,  3, 24, 45, 44,  2, 23, 22, 43,  1,  0, 21, 42 };
    const size_t SHUFFLE_INDEXES_SIZE = sizeof( SHUFFLE_INDEXES ) / sizeof( *SHUFFLE_INDEXES );

    const char BASE64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    const int HASH_LENGTH = 86;
}

QByteArray generateSalt( int length )
{
    QByteArray salt;
    salt.resize( length );
    for( int i = 0; i < length; ++i )
        salt[i] = BASE64[nx::utils::random::number() % (sizeof(BASE64)/sizeof(*BASE64) - 1)];

    return salt;
}

namespace
{
    const int BITS_MASK[] = { 0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F };

    //!Performs inplace bitshift of array data
    void shiftArrayRight( unsigned char* data, size_t dataSize, size_t bitsCount )
    {
        //TODO #ak moving by number of bytes and modifing bitsCount
        NX_ASSERT( bitsCount < 8 );

        if( bitsCount == 0 )
            return;

        int shiftedBitsBak = 0;
        for( size_t i = 0; i < dataSize; ++i )
        {
            //saving most right bits
            const int shiftedBitsNewBak = data[i] & BITS_MASK[bitsCount];
            data[i] >>= bitsCount;
            data[i] |= shiftedBitsBak << (8-bitsCount);
            shiftedBitsBak = shiftedBitsNewBak;
        }
    }
}

QByteArray linuxCryptSha512( const QByteArray& password, const QByteArray& salt )
{
    QByteArray intermediate = password + salt;
    QByteArray alternate = QCryptographicHash::hash( password + salt + password, QCryptographicHash::Sha512 );
    intermediate += alternate.mid( 0, password.size() );

    for( int i = password.size(); i != 0; i >>= 1 )
    {
        if( (i & 1) )
            intermediate += alternate;
        else
            intermediate += password;
    }
    intermediate = QCryptographicHash::hash( intermediate, QCryptographicHash::Sha512 );

    const unsigned char firstByte = intermediate[0];

    QByteArray p_bytes;
    for( int i = 0; i < password.size(); ++i )
        p_bytes += password;
    p_bytes = QCryptographicHash::hash( p_bytes, QCryptographicHash::Sha512 ).mid( 0, password.size() );

    QByteArray s_bytes;
    for( int i = 0; i < 16 + firstByte; ++i )
        s_bytes += salt;
    s_bytes = QCryptographicHash::hash( s_bytes, QCryptographicHash::Sha512 ).mid( 0, salt.size() );

    for( int i = 0; i<5000; ++i )
    {
        QByteArray result;
        if( i & 1 )
            result += p_bytes;
        else
            result += intermediate;
        if( i % 3 )
            result += s_bytes;
        if( i % 7 )
            result += p_bytes;
        if( i & 1 )
            result += intermediate;
        else
            result += p_bytes;
        intermediate = QCryptographicHash::hash( result, QCryptographicHash::Sha512 );
    }

    QByteArray rearranged;
    rearranged.resize( intermediate.size() );
    for( int i = 0; i < (int)SHUFFLE_INDEXES_SIZE; ++i )
        rearranged[i] = intermediate[SHUFFLE_INDEXES[i]];

    QByteArray hash;
    hash.reserve( HASH_LENGTH );
    for( int j = 0; j < HASH_LENGTH; ++j )
    {
        hash += BASE64[rearranged[rearranged.size() - 1] & BITS_MASK[6]];
        shiftArrayRight(
            reinterpret_cast<unsigned char*>(rearranged.data()),
            rearranged.size(),
            6 );
    }

    return "$6$" + salt + "$" + hash;
}

bool setRootPasswordDigest( const QByteArray& userName, const QByteArray& digest )
{
    if( userName.isEmpty() )
        return false;

#ifdef __linux__
    if( geteuid() != 0 )
        return false;   //we are not root

    QFile shadowFile( lit("/etc/shadow") );
    if( !shadowFile.open( QIODevice::ReadOnly ) )
        return false;

    QByteArray shadowFileContents = shadowFile.readAll();
    shadowFile.close();

    int userPos = 0;
    for( ;; )
    {
        userPos = shadowFileContents.indexOf( userName+":", userPos );
        if( userPos == -1 )
            return false; //user not found

        //checking that we have found complete word
        if( userPos != 0 && (isalnum(shadowFileContents[userPos-1]) || shadowFileContents[userPos - 1] == '_') )
        {
            //searching futher
            ++userPos;
            continue;
        }

        break;  //found user record
    }

    const int passwordHashStartPos = userPos + userName.size() + 1;

    //searching password hash end
    const int passwordHashEndPos = shadowFileContents.indexOf( ':', passwordHashStartPos );
    if( passwordHashEndPos == -1 )
        return false;   //bad file format

    //replacing password
    shadowFileContents.replace(
        passwordHashStartPos,
        passwordHashEndPos-passwordHashStartPos,
        digest );

    //saving modified file

    if( !shadowFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
        return false;

    return shadowFile.write( shadowFileContents ) == shadowFileContents.size();
#else
    NX_ASSERT( false );
    return false;
#endif
}
