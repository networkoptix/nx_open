#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{

struct ApiRebuildTransactionLogData: ApiData
{
	QString reserved;
};

#define ApiRebuildTransactionLogData_Fields (reserved)

}
