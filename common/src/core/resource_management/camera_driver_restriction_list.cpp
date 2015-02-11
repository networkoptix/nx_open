/**********************************************************
* 25 apr 2013
* akolesnikov
***********************************************************/

#include "camera_driver_restriction_list.h"

#include <utils/common/mutex.h>


static CameraDriverRestrictionList* CameraDriverRestrictionList_instance = nullptr;

CameraDriverRestrictionList::CameraDriverRestrictionList()
{
    assert( CameraDriverRestrictionList_instance == nullptr );
    CameraDriverRestrictionList_instance = this;
}

CameraDriverRestrictionList::~CameraDriverRestrictionList()
{
    CameraDriverRestrictionList_instance = nullptr;
}

void CameraDriverRestrictionList::allow( const QString& driverName, const QString& cameraVendor, const QString& cameraModelMask )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    std::vector<AllowRuleData>& rules = m_allowRulesByVendor[cameraVendor.toLower()];
    AllowRuleData ruleData;
    ruleData.modelNamePattern = QRegExp( cameraModelMask, Qt::CaseInsensitive, QRegExp::Wildcard );
    ruleData.driverName = driverName;
    rules.push_back( ruleData );
}

bool CameraDriverRestrictionList::driverAllowedForCamera( const QString& driverName, const QString& cameraVendor, const QString& cameraModel ) const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

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

CameraDriverRestrictionList* CameraDriverRestrictionList::instance()
{
    return CameraDriverRestrictionList_instance;
}
