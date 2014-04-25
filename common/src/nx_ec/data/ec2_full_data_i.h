    struct ApiFullInfoData: ApiData {
        ApiResourceTypeDataListData resTypes;
        ApiMediaServerList servers;
        ApiCameraList cameras;
        ApiUserList users;
        ApiLayoutList layouts;
        ApiVideowallList videowalls;
        ApiBusinessRuleList rules;
        ApiCameraServerItemList cameraHistory;
        ApiLicenseList licenses;
        ServerInfo serverInfo;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiFullInfoData, (resTypes) (servers) (cameras) (users) (layouts) (videowalls) (rules) (cameraHistory) (licenses) (serverInfo) )
    QN_DEFINE_STRUCT_SERIALIZATORS (ServerInfo, (mainHardwareIds) (compatibleHardwareIds) (publicIp) (systemName) (sessionKey) (allowCameraChanges) (armBox))
