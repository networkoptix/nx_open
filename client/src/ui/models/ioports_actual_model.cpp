#include "ioports_actual_model.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include "ui/workbench/workbench_context.h"

QnIOPortsActualModel::QnIOPortsActualModel(QObject *parent): 
    base_type(parent)
{
    //connect(QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::resourceParamChanged, this, &QnIOPortsActualModel::at_propertyChanged);
}

void QnIOPortsActualModel::at_propertyChanged(const QnResourcePtr& /*res*/, const QString& /*key*/)
{

}
