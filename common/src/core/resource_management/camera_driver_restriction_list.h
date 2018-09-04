#pragma once

#include <map>
#include <vector>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QRegExp>
#include <QtCore/QString>


//!Knows which driver is allowed to process any given camera, and which is not
/*!
    External drivers (camera integration plugins) are allowed to override built-in drivers
    \note By default, everything is allowed
    \note Camera vendor name and model name are case insensitive
*/
class CameraDriverRestrictionList: public QObject
{
public:
    using QObject::QObject;

    //!Remembers, that only \a driverName is allowed to process camera specified by \a cameraVendor : \a cameraModelMask
    /*!
        \param cameraModelMask Wildcard (* and ? are allowed) mask of model name
    */
    void allow( const QString& driverName, const QString& cameraVendor, const QString& cameraModelMask );
    //!Returns true, if driver \a driverName is allowed to process camera \a cameraVendor : \a cameraModel
    bool driverAllowedForCamera( const QString& driverName, const QString& cameraVendor, const QString& cameraModel ) const;

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
