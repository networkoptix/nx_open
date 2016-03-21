/**********************************************************
* Dec 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_MS_SYSTEM_SETTINGS_HANDLER_H
#define NX_MS_SYSTEM_SETTINGS_HANDLER_H

#include <rest/server/json_rest_handler.h>


/*!
    If no parameters are specified then just returns list of settings.
    If setting(s) to set is specified, then new values are returned
*/
class QnSystemSettingsHandler
:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*) override;
};

#endif  //NX_MS_SYSTEM_SETTINGS_HANDLER_H
