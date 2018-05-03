/**********************************************************
* 9 jun 2015
* a.kolesnikov
***********************************************************/

#ifndef _nx_linux_passwd_crypt_h
#define _nx_linux_passwd_crypt_h

#include <QtCore/QByteArray>


static const size_t LINUX_CRYPT_SALT_LENGTH = 8;

QByteArray NX_UTILS_API generateSalt( int length );
//!Calculates sha512 hash suitable for /etc/shadow file
QByteArray NX_UTILS_API linuxCryptSha512( const QByteArray& password, const QByteArray& salt );


/*!
    Currently implemented on linux only. Modifies /etc/shadow. digest MUST be suitable for a shadow file
    NOTE: on linux process MUST have root priviledge to be able to modify /etc/shadow
*/
bool NX_UTILS_API setRootPasswordDigest( const QByteArray& userName, const QByteArray& digest );

#endif  //_nx_linux_passwd_crypt_h
