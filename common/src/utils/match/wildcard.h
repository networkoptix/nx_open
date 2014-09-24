////////////////////////////////////////////////////////////
// 2 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef WILDCARD_H
#define WILDCARD_H

#include <QString>


//!Validates \a str for appliance to wild-card expression \a mask
/*!
    \param mask Wildcard expression. Allowed to contain special symbols ? (any character except .), * (any number of any characters)
    \param str String being validated for appliance to \a mask
    \return true, if \a str has been validated with \a mask. false, otherwise
*/
bool wildcardMatch( const char* mask, const char* str );

inline bool wildcardMatch( const QString& mask, const QString& str )
{
    return wildcardMatch( mask.toLatin1().constData(), str.toLatin1().constData() );
}

#endif  //WILDCARD_H
