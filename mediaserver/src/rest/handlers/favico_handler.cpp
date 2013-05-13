#include "favico_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"

static const int READ_BLOCK_SIZE = 1024*512;

QnRestFavicoHandler::QnRestFavicoHandler()
{

}

int QnRestFavicoHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    QFile f(":/hdw_logo.ico");
    if (f.open(QFile::ReadOnly)) 
    {
        result = f.readAll();
        contentType = "image/vnd.microsoft.icon";
        return CODE_OK;
    }
    else {
        return CODE_NOT_FOUND;
    }
}

int QnRestFavicoHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnRestFavicoHandler::description(TCPSocket *) const
{
    return "Returns favico";
}
