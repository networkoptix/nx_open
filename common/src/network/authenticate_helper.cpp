#include "authenticate_helper.h"

#include <QUuid>
#include <QElapsedTimer>
#include "core/resource_managment/resource_pool.h"
#include "core/resource/user_resource.h"
#include "utils/common/util.h"

QnAuthHelper* QnAuthHelper::m_instance;

static const int NONCE_TIMEOUT = 1000 * 60 * 5;
static const QString REALM(lit("NetworkOptix"));

QnAuthHelper::QnAuthHelper()
{
    connect(qnResPool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
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

bool QnAuthHelper::authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders)
{
    removeOldNonces();

    // implement me
    QString authorization = headers.value(lit("Authorization"));
    if (authorization.isEmpty()) {
        addAuthHeader(responseHeaders);
        return false;
    }

    QString authType;
    QString authData;
    int pos = authorization.indexOf(L' ');
    if (pos > 0) {
        authType = authorization.left(pos).toLower();
        authData = authorization.mid(pos+1);
    }
    if (authType == lit("digest"))
        return doDigestAuth(headers.method().toUtf8(), authData.toUtf8(), responseHeaders);
    else if (authType == lit("basic"))
        return doBasicAuth(authData.toUtf8(), responseHeaders);
    else
        return false;
}

bool QnAuthHelper::doDigestAuth(const QByteArray& method, const QByteArray& authData, QHttpResponseHeader& responseHeaders)
{
    QList<QByteArray> authParams = authData.split(',');

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
            userName = value;
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

    QMutexLocker lock(&m_mutex);

    if (!m_nonces.contains(nonce))
        return false;

    foreach(QnUserResourcePtr user, m_users)
    {
        if (user->getName().toUtf8() == userName)
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
    return false;
}

bool QnAuthHelper::doBasicAuth(const QByteArray& authData, QHttpResponseHeader& responseHeaders)
{
    QByteArray digest = QByteArray::fromBase64(authData);
    int pos = digest.indexOf(':');
    if (pos == -1)
        return false;
    QByteArray userName = digest.left(pos);
    QByteArray password = digest.mid(pos+1);

    foreach(QnUserResourcePtr user, m_users)
    {
        if (user->getName().toUtf8() == userName)
        {
            QList<QByteArray> pswdData = user->getPassword().toUtf8().split('$');
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

void QnAuthHelper::addAuthHeader(QHttpResponseHeader& responseHeaders)
{
    //QString auth(lit("Digest realm=\"%1\",nonce=\"%2\""));
    //responseHeaders.setValue(lit("WWW-Authenticate"), auth.arg(REALM).arg(getNonce()));
    QString auth(lit("Digest realm=\"%1\",nonce=\"%2\""));
    responseHeaders.setValue(lit("WWW-Authenticate"), auth.arg(REALM).arg(lit(getNonce())));
}

void QnAuthHelper::removeOldNonces()
{
    QMutexLocker lock(&m_mutex);
    for(QMap<QByteArray, QElapsedTimer>::iterator itr = m_nonces.begin(); itr != m_nonces.end();)
    {
        if (itr.value().elapsed() >= NONCE_TIMEOUT)
            itr = m_nonces.erase(itr);
        else
            ++itr;
    }
}

QByteArray QnAuthHelper::getNonce()
{
    QByteArray rez = QByteArray::number(getUsecTimer(), 16);
    //QString rez = QUuid::createUuid().toString();
    //rez = rez.mid(1, rez.length() - 2);

    QMutexLocker lock(&m_mutex);
    QMap<QString, QElapsedTimer>::iterator itr = m_nonces.insert(rez, QElapsedTimer());
    itr.value().restart();
    return rez;
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
