struct QnConnectionInfoData {
    QnConnectionInfoData(): proxyPort(0) {}

    QUrl ecUrl;
    SoftwareVersionType version;
    QList<QnCompatibilityItem> compatibilityItems;
    int proxyPort;
    QString ecsGuid;
    QString publicIp;
    QString brand;
};
QN_DEFINE_STRUCT_SERIALIZATORS (QnConnectionInfoData, (ecUrl)(version)(compatibilityItems)(proxyPort)(ecsGuid)(publicIp)(brand))