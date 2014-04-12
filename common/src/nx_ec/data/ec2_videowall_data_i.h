struct ApiVideowallItemData {
    QnId guid;
    QnId pc_guid;
    QnId layout_guid;
    QString name;
    int x;
    int y;
    int w;
    int h;
};

#define ApiVideowallItemDataFields (guid) (pc_guid) (layout_guid) (name) (x) (y) (w) (h)
QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallItemData, ApiVideowallItemDataFields)

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
};

#define ApiVideowallScreenDataFields (pc_guid) (pc_index) (desktop_x) (desktop_y) (desktop_w) (desktop_h) (layout_x) (layout_y) (layout_w) (layout_h)
QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallScreenData, ApiVideowallScreenDataFields)

struct ApiVideowallData: virtual ApiResourceData
{
    ApiVideowallData(): autorun(false) {}

    bool autorun;

    std::vector<ApiVideowallItem> items;
    std::vector<ApiVideowallScreen> screens;
};

#define ApiVideowallDataFields (autorun) (items) (screens)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(ApiVideowallData, ApiResourceData, ApiVideowallDataFields);
QN_DEFINE_API_OBJECT_LIST_DATA(ApiVideowall)

struct ApiVideowallControlMessageData {
    int operation;
    QnId videowall_guid;
    QnId instance_guid;
    std::map<QString, QString> params;
};

#define ApiVideowallControlMessageDataFields (operation) (videowall_guid) (instance_guid) (params)
QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallControlMessageData, ApiVideowallControlMessageDataFields)
