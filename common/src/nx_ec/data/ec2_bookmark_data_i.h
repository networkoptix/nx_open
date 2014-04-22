    struct ApiCameraBookmarkData {
        QnId guid;
        qint64 startTime;
        qint64 duration;
        QString name;
        QString description;
        qint64 timeout;
        std::vector<QString> tags;
    };
    #define ApiCameraBookmarkFields (guid) (startTime) (duration) (name) (description) (timeout) (tags)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiCameraBookmarkData, ApiCameraBookmarkFields);