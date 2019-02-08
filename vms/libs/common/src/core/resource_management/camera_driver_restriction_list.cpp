#include "camera_driver_restriction_list.h"

#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/assert.h>

void CameraDriverRestrictionList::allow( const QString& driverName, const QString& cameraVendor, const QString& cameraModelMask )
{
    QnMutexLocker lk( &m_mutex );

    std::vector<AllowRuleData>& rules = m_allowRulesByVendor[cameraVendor.toLower()];
    AllowRuleData ruleData;
    ruleData.modelNamePattern = QRegExp( cameraModelMask, Qt::CaseInsensitive, QRegExp::Wildcard );
    ruleData.driverName = driverName;
    rules.push_back( ruleData );
}

bool CameraDriverRestrictionList::driverAllowedForCamera( const QString& driverName, const QString& cameraVendor, const QString& cameraModel ) const
{
    QnMutexLocker lk( &m_mutex );

    std::map<QString, std::vector<AllowRuleData> >::const_iterator it = m_allowRulesByVendor.find(cameraVendor.toLower());
    if( it == m_allowRulesByVendor.end() )
        return true;

    const std::vector<AllowRuleData>& rules = it->second;
    for( std::vector<AllowRuleData>::size_type i = 0; i < rules.size(); ++i )
    {
        if( rules[i].modelNamePattern.exactMatch(cameraModel) )
            return rules[i].driverName == driverName;
    }
    return true;    //by default, everything is allowed
}
