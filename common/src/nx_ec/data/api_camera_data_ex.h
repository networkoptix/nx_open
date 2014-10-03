/**********************************************************
* 2 oct 2014
* akolesnikov
***********************************************************/

#ifndef __API_CAMERA_DATA_EX_H_
#define __API_CAMERA_DATA_EX_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_camera_data.h"
#include "api_camera_attributes_data.h"

namespace ec2
{
    struct ApiCameraDataEx
    :
        ApiCameraData,
        ApiCameraAttributesData
    {
    };
#define ApiCameraDataEx_Fields ApiCameraData_Fields ApiCameraAttributesData_Fields

} // namespace ec2

#endif // __API_CAMERA_DATA_EX_H_
