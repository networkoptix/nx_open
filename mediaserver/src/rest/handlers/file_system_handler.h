#ifndef QN_FILE_SYSTEM_HANDLER_H
#define QN_FILE_SYSTEM_HANDLER_H

#include <rest/server/json_rest_handler.h>

// TODO: #Elric QnStorageStatusHandler
class QnFileSystemHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) override;
    virtual QString description() const override;
};

#endif // QN_FILE_SYSTEM_HANDLER_H
