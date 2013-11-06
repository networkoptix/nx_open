
#include "authenticate_helper.h"

#include <QtCore/QUuid>
#include <QtCore/QCryptographicHash>
#include "core/resource_managment/resource_pool.h"
#include "core/resource/user_resource.h"
#include "utils/common/util.h"
#include "utils/common/synctime.h"
#include "api/app_server_connection.h"

QnAuthHelper* QnAuthHelper::m_instance;

static const qint64 NONCE_TIMEOUT = 1000000ll * 60 * 5;
static const QString REALM(lit("NetworkOptix"));

QnAuthHelper::QnAuthHelper()
{
    connect(qnResPool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceChanged(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(qnResPool, SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
}

QnAuthHelper::~QnAuthHelper()
{
    disconnect(qnResPool, NULL, this, NULL);
}

void QnAuthHelper::initStaticInstance(QnAuthHelper* instance)
{
    delete m_instance;
    m_instance = instance;
}

QnAuthHelper* QnAuthHelper::instance()
{
    return m_instance;
}

bool QnAuthHelper::authenticate(const nx_http::HttpRequest& request, nx_http::HttpResponse& response)
{
    QString cookie = QLatin1String(nx_http::getHeaderValue( request.headers, "Cookie" ));
    int customAuthInfoPos = cookie.indexOf(lit("authinfo="));
    if (customAuthInfoPos >= 0) {
        QString digest = cookie.mid(customAuthInfoPos + QByteArray("authinfo=").length(), 32);
        if (doCustomAuthorization(digest.toLatin1(), response, QnAppServerConnectionFactory::sessionKey()))
            return true;
        if (doCustomAuthorization(digest.toLatin1(), response, QnAppServerConnectionFactory::prevSessionKey()))
            return true;
    }

    nx_http::StringType authorization = nx_http::getHeaderValue( request.headers, "Authorization" );
    if (authorization.isEmpty()) {
        addAuthHeader(response);
        return false;
    }

    nx_http::StringType authType;
    nx_http::StringType authData;
    int pos = authorization.indexOf(L' ');
    if (pos > 0) {
        authType = authorization.left(pos).toLower();
        authData = authorization.mid(pos+1);
    }
    if (authType == "digest")
        return doDigestAuth( request.requestLine.method, authData, response);
    else if (authType == "basic")
        return doBasicAuth(authData, response);
    else
        return false;
}

//!Splits \a data by \a delimiter not closed within quotes
/*!
    E.g.: 
    \code
    one, two, "three, four"
    \endcode

    will be splitted to 
    \code
    one
    two
    "three, four"
    \endcode
*/
static QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter)
{
    bool quoted = false;
    QList<QByteArray> rez;
    if (data.isEmpty())
        return rez;

    int lastPos = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i] == '\"')
            quoted = !quoted;
        else if (data[i] == delimiter && !quoted)
        {
            rez << QByteArray(data.constData() + lastPos, i - lastPos);
            lastPos = i + 1;
        }
    }
    rez << QByteArray(data.constData() + lastPos, data.size() - lastPos);

    return rez;
}

bool QnAuthHelper::doDigestAuth(const QByteArray& method, const QByteArray& authData, nx_http::HttpResponse& responseHeaders)
{
    const QList<QByteArray>& authParams = smartSplit(authData, ',');

    QByteArray userName;
    QByteArray response;
    QByteArray nonce;
    QByteArray realm;
    QByteArray uri;

    for (int i = 0; i < authParams.size(); ++i)
    {
        QByteArray data = authParams[i].trimmed();
        int pos = data.indexOf('=');
        if (pos == -1)
            return false;
        QByteArray key = data.left(pos);
        QByteArray value = data.mid(pos+1);
        value = value.mid(1, value.length()-2);
        if (key == "username")
            userName = value.toLower();
        else if (key == "response")
            response = value;
        else if (key == "nonce")
            nonce = value;
        else if (key == "realm")
            realm = value;
        else if (key == "uri")
            uri = value;
    }

    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(method);
    md5Hash.addData(":");
    md5Hash.addData(uri);
    QByteArray ha2 = md5Hash.result().toHex();

    if (isNonceValid(nonce)) 
    {
        QMutexLocker lock(&m_mutex);
        foreach(QnUserResourcePtr user, m_users)
        {
            if (user->getName().toUtf8().toLower() == userName)
            {
                QByteArray dbHash = user->getDigest().toUtf8();

                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(dbHash);
                md5Hash.addData(":");
                md5Hash.addData(nonce);
                md5Hash.addData(":");
                md5Hash.addData(ha2);
                QByteArray calcResponse = md5Hash.result().toHex();

                if (calcResponse == response)
                    return true;
            }
        }
    }

    addAuthHeader(responseHeaders);
    return false;
}

bool QnAuthHelper::doBasicAuth(const QByteArray& authData, nx_http::HttpResponse& /*response*/)
{
    QByteArray digest = QByteArray::fromBase64(authData);
    int pos = digest.indexOf(':');
    if (pos == -1)
        return false;
    QByteArray userName = digest.left(pos).toLower();
    QByteArray password = digest.mid(pos+1);

    foreach(QnUserResourcePtr user, m_users)
    {
        if (user->getName().toUtf8().toLower() == userName)
        {
            QList<QByteArray> pswdData = user->getHash().toUtf8().split('$');
            if (pswdData.size() == 3)
            {
                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(pswdData[1]);
                md5Hash.addData(password);
                QByteArray incomeHash = md5Hash.result().toHex();
                if (incomeHash == pswdData[2])
                    return true;
            }
        }
    }
    return false;
}

bool QnAuthHelper::doCustomAuthorization(const QByteArray& authData, nx_http::HttpResponse& /*response*/, const QByteArray& sesionKey)
{
    QByteArray digestBin = QByteArray::fromHex(authData);
    QByteArray sessionKeyBin = QByteArray::fromHex(sesionKey);
    int size = qMin(digestBin.length(), sessionKeyBin.length());
    for (int i = 0; i < size; ++i)
        digestBin[i] = digestBin[i] ^ sessionKeyBin[i];
    QByteArray digest = digestBin.toHex();

    foreach(QnUserResourcePtr user, m_users)
    {
        if (user->getDigest().toLatin1() == digest)
            return true;
    }
    return false;
}

void QnAuthHelper::addAuthHeader(nx_http::HttpResponse& response)
{
    QString auth(lit("Digest realm=\"%1\",nonce=\"%2\""));
    response.headers.insert( nx_http::HttpHeader( "WWW-Authenticate", auth.arg(REALM).arg(lit(getNonce())).toLatin1() ) );
}

bool QnAuthHelper::isNonceValid(const QByteArray& nonce) const
{
    qint64 n1 = nonce.toLongLong(0, 16);
    qint64 n2 = qnSyncTime->currentUSecsSinceEpoch();
    return qAbs(n2 - n1) < NONCE_TIMEOUT;
}

QByteArray QnAuthHelper::getNonce()
{
    return QByteArray::number(qnSyncTime->currentUSecsSinceEpoch() , 16);
}

void QnAuthHelper::at_resourcePool_resourceAdded(const QnResourcePtr & res)
{
    QMutexLocker lock(&m_mutex);

    QnUserResourcePtr user = res.dynamicCast<QnUserResource>();
    if (user)
        m_users.insert(user->getId(), user);
}

void QnAuthHelper::at_resourcePool_resourceRemoved(const QnResourcePtr &res)
{
    QMutexLocker lock(&m_mutex);

    m_users.remove(res->getId());
}
