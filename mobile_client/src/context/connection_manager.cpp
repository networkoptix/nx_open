#include "connection_manager.h"

QnConnectionManager::QnConnectionManager(QObject *parent) :
    QObject(parent)
{
}

void QnConnectionManager::connect(const QUrl &url) {
    emit connected();
}

void QnConnectionManager::disconnect(bool force) {

}
