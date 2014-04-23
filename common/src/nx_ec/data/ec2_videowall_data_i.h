struct ApiVideowallItemData {
    QnId guid;
    QnId pc_guid;
    QnId layout_guid;
    QString name;
    int x;
    int y;
    int w;
    int h;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

#define ApiVideowallItemDataFields (guid) (pc_guid) (layout_guid) (name) (x) (y) (w) (h)
//QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallItemData, ApiVideowallItemDataFields)
QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallItemData, (binary))

struct ApiVideowallScreenData {
    QnId pc_guid;
    int pc_index;
    int desktop_x;
    int desktop_y;
    int desktop_w;
    int desktop_h;
    int layout_x;
    int layout_y;
    int layout_w;
    int layout_h;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

#define ApiVideowallScreenDataFields (pc_guid) (pc_index) (desktop_x) (desktop_y) (desktop_w) (desktop_h) (layout_x) (layout_y) (layout_w) (layout_h)
//QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallScreenData, ApiVideowallScreenDataFields)
QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallScreenData, (binary))

struct ApiVideowallData: ApiResourceData
{
    ApiVideowallData(): autorun(false) {}

    bool autorun;

    std::vector<ApiVideowallItemData> items;
    std::vector<ApiVideowallScreenData> screens;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

#define ApiVideowallDataFields (autorun) (items) (screens)
//QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiVideowallData, ApiResourceData, ApiVideowallDataFields);
QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallData, (binary))

QN_DEFINE_API_OBJECT_LIST_DATA(ApiVideowallData)

struct ApiVideowallControlMessageData {
    int operation;
    QnId videowall_guid;
    QnId instance_guid;
    std::map<QString, QString> params;
};

#define ApiVideowallControlMessageDataFields (operation) (videowall_guid) (instance_guid) (params)
//QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallControlMessageData, ApiVideowallControlMessageDataFields)
QN_FUSION_DECLARE_FUNCTIONS(ApiVideowallControlMessageData, (binary))
