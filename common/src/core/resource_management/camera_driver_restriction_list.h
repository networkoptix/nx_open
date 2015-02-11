/**********************************************************
* 25 apr 2013
* akolesnikov
***********************************************************/

#ifndef CAMERA_DRIVER_RESTRICTION_LIST_H
#define CAMERA_DRIVER_RESTRICTION_LIST_H

#include <map>
#include <vector>

#include <utils/common/mutex.h>
#include <QtCore/QRegExp>
#include <QtCore/QString>


//!Knows which driver is allowed to process any given camera, and which is not
/*!
    External drivers (camera integration plugins) are allowed to override built-in drivers
    \note By default, everything is allowed
    \note Camera vendor name and model name are case insensitive
*/
class CameraDriverRestrictionList
{
public:
    CameraDriverRestrictionList();
    ~CameraDriverRestrictionList();

    //!Remembers, that only \a driverName is allowed to process camera specified by \a cameraVendor : \a cameraModelMask
    /*!
        \param cameraModelMask Wildcard (* and ? are allowed) mask of model name
    */
    void allow( const QString& driverName, const QString& cameraVendor, const QString& cameraModelMask );
    //!Returns true, if driver \a driverName is allowed to process camera \a cameraVendor : \a cameraModel
    bool driverAllowedForCamera( const QString& driverName, const QString& cameraVendor, const QString& cameraModel ) const;

    static CameraDriverRestrictionList* instance();

private:
    struct AllowRuleData
    {
        QRegExp modelNamePattern;
        QString driverName;
    };

    //!map<vendor name, array of rules>
    std::map<QString, std::vector<AllowRuleData> > m_allowRulesByVendor;
    mutable QnMutex m_mutex;
};

#endif  //CAMERA_DRIVER_RESTRICTION_LIST_H
