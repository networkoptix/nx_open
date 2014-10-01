/**********************************************************
* 21 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_TZ_H
#define NX_TZ_H


namespace nx_tz
{
    //!Returns offset (in minutes) from UTC to local timezone. Timezones west of UTC have negative offset, to the east - positive offset
    /*!
        \return in case of error returns -1
    */
    int getLocalTimeZoneOffset();
}

#endif  //NX_TZ_H
