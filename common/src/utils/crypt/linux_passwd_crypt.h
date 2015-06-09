/**********************************************************
* 9 jun 2015
* a.kolesnikov
***********************************************************/

#ifndef _nx_linux_passwd_crypt_h
#define _nx_linux_passwd_crypt_h

#include <QtCore/QByteArray>


static const size_t LINUX_CRYPT_SALT_LENGTH = 8;

QByteArray generateSalt( int length );
//!Calculates sha512 hash suitable for /etc/shadow file
QByteArray linuxCryptSha512( const QByteArray& password, const QByteArray& salt );

#endif  //_nx_linux_passwd_crypt_h
