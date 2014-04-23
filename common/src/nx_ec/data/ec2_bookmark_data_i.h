    struct ApiCameraBookmarkData: ApiData {
        QnId guid;
        qint64 startTime;
        qint64 duration;
        QString name;
        QString description;
        qint64 timeout;
        std::vector<QString> tags;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };
    #define ApiCameraBookmarkFields (guid) (startTime) (duration) (name) (description) (timeout) (tags)
    QN_DEFINE_STRUCT_SERIALIZATORS(ApiCameraBookmarkData, ApiCameraBookmarkFields);

    struct ApiCameraBookmarkTag: ApiData {
        QnId bookmarkGuid; 
        QString name;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };
    #define ApiCameraBookmarkTagFields (bookmarkGuid) (name)