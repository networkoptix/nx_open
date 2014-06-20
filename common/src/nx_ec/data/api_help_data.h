#ifndef __API_HELP_DATA_H
#define __API_HELP_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiHelpValueData: public ApiData
    {
        QString name;
        QString description;
    };
    #define ApiHelpValueData_Fields (name)(description)

    struct ApiHelpParamData: public ApiData
    {
        ApiHelpParamData(): optional(false) {}
        ApiHelpParamData(const QString& name, const QString& description, bool optional): name(name), description(description), optional(optional) {}
        
        QString name;
        QString description;
        bool optional;
        std::vector<ApiHelpValueData> values;
    };
    #define ApiHelpParamData_Fields (name)(description)(optional)(values)

    struct ApiHelpFunctionData: public ApiData
    {
        ApiHelpFunctionData(): isGetter(false) {}
        
        QString name;
        QString description;
        bool isGetter;
        std::vector<ApiHelpParamData> params;
        QString result; // description for function result
    };
    #define ApiHelpFunctionData_Fields (name)(description)(isGetter)(params)(result)

    struct ApiHelpGroupData: public ApiData
    {
        QString groupName;
        QString groupDescription;
        std::vector<ApiHelpFunctionData> methods;
    };
    #define ApiHelpGroupData_Fields (groupName)(groupDescription)(methods)

    struct ApiHelpGroupDataList: public ApiData
    {
        std::vector<ApiHelpGroupData> groups;
    };
    #define ApiHelpGroupDataList_Fields (groups)

} // namespace ec2


#endif // __API_HELP_DATA_H
