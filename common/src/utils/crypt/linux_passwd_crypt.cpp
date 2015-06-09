/**********************************************************
* 9 jun 2015
* a.kolesnikov
***********************************************************/

#include "linux_passwd_crypt.h"

#include <boost/multiprecision/cpp_int.hpp>

#include <QCryptographicHash>


namespace 
{
    const int SHUFFLE_INDEXES[] =
    { 63, 62, 20, 41, 40, 61, 19, 18, 39, 60, 59, 17, 38, 37, 58, 16, 15, 36, 57, 56, 14, 35,
          34, 55, 13, 12, 33, 54, 53, 11, 32, 31, 52, 10,  9, 30, 51, 50,  8, 29, 28, 49,  7,
          6,  27, 48, 47,  5, 26, 25, 46,  4,  3, 24, 45, 44,  2, 23, 22, 43,  1,  0, 21, 42 };
    const char BASE64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int HASH_LENGTH = 86;
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

    unsigned char firstByte = intermediate[0];

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

    boost::multiprecision::int512_t rearrangedInt( 0 );
    for( size_t i = 0; i < sizeof( SHUFFLE_INDEXES ) / sizeof( *SHUFFLE_INDEXES ); ++i )
    {
        rearrangedInt <<= 8;
        rearrangedInt |= (unsigned char)(intermediate[SHUFFLE_INDEXES[i]]);
    }

    QByteArray hash;
    hash.reserve( HASH_LENGTH );
    for( int j = 0; j < HASH_LENGTH; ++j )
    {
        hash += BASE64[(rearrangedInt & 0x3f).convert_to<int>()];
        rearrangedInt >>= 6;    //div by 64
    }

    return "$6$" + salt + "$" + hash;
}
