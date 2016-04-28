package com.networkoptix.hdwitness.api.ec2;

import com.networkoptix.hdwitness.BuildConfig;

public final class TransactionCommands {
    public static final int             NotDefined                  = 0;
    
    /* System */
    public static final int             tranSyncRequest             = 1;    /*< ApiSyncRequestData */
    public static final int             tranSyncResponse            = 2;    /*< QnTranStateResponse */       
    public static final int             lockRequest                 = 3;    /*< ApiLockData */
    public static final int             lockResponse                = 4;    /*< ApiLockData */
    public static final int             unlockRequest               = 5;    /*< ApiLockData */
    public static final int             peerAliveInfo               = 6;    /*< ApiPeerAliveData */
    public static final int             tranSyncDone                = 7;    /*< ApiTranSyncDoneData */
    
    /* Connection */
    public static final int             testConnection              = 100;  /*< ApiLoginData */
    public static final int             connect                     = 101;  /*< ApiLoginData */
    
    /* Common resource */
    public static final int             saveResource                = 200;  /*< ApiResourceData */
    public static final int             removeResource              = 201;  /*< ApiIdData */
    public static final int             setResourceStatus           = 202;  /*< ApiResourceStatusData */
    public static final int             getResourceParams           = 203;  /*< ApiResourceParamDataList */
    public static final int             setResourceParams           = 204;  /*< ApiResourceParamWithRefDataList */
    public static final int             getResourceTypes            = 205;  /*< ApiResourceTypeDataList*/
    public static final int             getFullInfo                 = 206;  /*< ApiFullInfoData */
    public static final int             setPanicMode                = 207;  /*< ApiPanicModeData */
    public static final int             setResourceParam            = 208;  /*< ApiResourceParamWithRefData */
    public static final int             removeResourceParam         = 209;  /*< ApiResourceParamWithRefData */
    public static final int             removeResourceParams        = 210;  /*< ApiResourceParamWithRefDataList */
    public static final int             getStatusList               = 211;  /*< ApiResourceStatusDataList */
    public static final int             removeResources             = 212;  /*< ApiIdDataList */
    
    /* Camera resource */
    public static final int             getCameras                  = 300;  /*< ApiCameraDataList */
    public static final int             saveCamera                  = 301;  /*< ApiCameraData */
    public static final int             saveCameras                 = 302;  /*< ApiCameraDataList */
    public static final int             removeCamera                = 303;  /*< ApiIdData */
    public static final int             getCameraHistoryItems       = 304;  /*< ApiCameraServerItemDataList */
    public static final int             addCameraHistoryItem        = 305;  /*< ApiCameraServerItemData */
    public static final int             addCameraBookmarkTags       = 306;  /*< ApiCameraBookmarkTagDataList */
    public static final int             getCameraBookmarkTags       = 307;  /*< ApiCameraBookmarkTagDataList */
    public static final int             removeCameraBookmarkTags    = 308;
    public static final int             removeCameraHistoryItem     = 309;  /*< ApiCameraServerItemData */
    public static final int             saveCameraUserAttributes    = 310;  /*< ApiCameraAttributesData */
    public static final int             saveCameraUserAttributesList= 311;  /*< ApiCameraAttributesDataList */
    public static final int             getCameraUserAttributes     = 312;  /*< ApiCameraAttributesDataList */
    public static final int             getCamerasEx                = 313;  /*< ApiCameraDataExList */
                          
    /* MediaServer resource */
    public static final int             getMediaServers             = 400;  /*< ApiMediaServerDataList */
    public static final int             saveMediaServer             = 401;  /*< ApiMediaServerData */
    public static final int             removeMediaServer           = 402;  /*< ApiIdData */
    public static final int             saveServerUserAttributes    = 403;  /*< QnMediaServerUserAttributesList */
    public static final int             saveServerUserAttributesList= 404;  /*< QnMediaServerUserAttributesList */
    public static final int             getServerUserAttributes     = 405;  /*< ApiIdData, QnMediaServerUserAttributesList */
    public static final int             removeServerUserAttributes  = 406;  /*< ApiIdData */
    public static final int             saveStorage                 = 407;  /*< ApiStorageData */
    public static final int             saveStorages                = 408;  /*< ApiStorageDataList */
    public static final int             removeStorage               = 409;  /*< ApiIdData */
    public static final int             removeStorages              = 410;  /*< QList<ApiIdData> */
    public static final int             getMediaServersEx           = 411;  /*< ApiMediaServerDataExList */
    public static final int             getStorages                 = 412;  /*< ApiStorageDataList */
    
    /* User resource */
    public static final int             getUsers                    = 500;  /*< ApiUserDataList */
    public static final int             saveUser                    = 501;  /*< ApiUserData */
    public static final int             removeUser                  = 502;  /*< ApiIdData */
    
    /* Layout resource */
    public static final int             getLayouts                  = 600;  /*< ApiLayoutDataList */
    public static final int             saveLayout                  = 601;  /*< ApiLayoutDataList */
    public static final int             saveLayouts                 = 602;  /*< ApiLayoutData */
    public static final int             removeLayout                = 603;  /*< ApiIdData */
    
    /* Videowall resource */
    public static final int             getVideowalls               = 700;  /*< ApiVideowallDataList */
    public static final int             saveVideowall               = 701;  /*< ApiVideowallData */
    public static final int             removeVideowall             = 702;  /*< ApiIdData */
    public static final int             videowallControl            = 703;  /*< ApiVideowallControlMessageData */
    
    /* Business rules */
    public static final int             getBusinessRules            = 800;  /*< ApiBusinessRuleDataList */
    public static final int             saveBusinessRule            = 801;  /*< ApiBusinessRuleData */
    public static final int             removeBusinessRule          = 802;  /*< ApiIdData */
    public static final int             resetBusinessRules          = 803;  /*< ApiResetBusinessRuleData */
    public static final int             broadcastBusinessAction     = 804;  /*< ApiBusinessActionData */
    public static final int             execBusinessAction          = 805;  /*< ApiBusinessActionData */
    
    /* Stored files */
    public static final int             listDirectory               = 900;  /*< ApiStoredDirContents */
    public static final int             getStoredFile               = 901;  /*< ApiStoredFileData */
    public static final int             addStoredFile               = 902;  /*< ApiStoredFileData */
    public static final int             updateStoredFile            = 903;  /*< ApiStoredFileData */
    public static final int             removeStoredFile            = 904;  /*< ApiStoredFilePath */
    
    /* Licenses */
    public static final int             getLicenses                 = 1000; /*< ApiLicenseDataList */
    public static final int             addLicense                  = 1001; /*< ApiLicenseData */
    public static final int             addLicenses                 = 1002; /*< ApiLicenseDataList */
    public static final int             removeLicense               = 1003; /*< ApiLicenseData */
    
    /* Email */
    public static final int             testEmailSettings           = 1100; /*< ApiEmailSettingsData */
    public static final int             sendEmail                   = 1101; /*< ApiEmailData */
    
    /* Auto-updates */
    public static final int             uploadUpdate                = 1200; /*< ApiUpdateUploadData */
    public static final int             uploadUpdateResponce        = 1201; /*< ApiUpdateUploadResponceData */
    public static final int             installUpdate               = 1202; /*< ApiUpdateInstallData  */
    
    /* Module information */
    public static final int             moduleInfo                  = 1301; /*< ApiModuleData */
    public static final int             moduleInfoList              = 1302; /*< ApiModuleDataList */
    
    /* Discovery */
    public static final int             discoverPeer                = 1401; /*< ApiDiscoveryData */
    public static final int             addDiscoveryInformation     = 1402; /*< ApiDiscoveryData*/
    public static final int             removeDiscoveryInformation  = 1403; /*< ApiDiscoveryData*/
    public static final int             getDiscoveryData            = 1404; /*< ApiDiscoveryDataList */
    
    /* Misc */
    public static final int             forcePrimaryTimeServer      = 2001;  /*< ApiIdData */
    public static final int             broadcastPeerSystemTime     = 2002;  /*< ApiPeerSystemTimeData*/
    public static final int             getCurrentTime              = 2003;  /*< ApiTimeData */         
    public static final int             changeSystemName            = 2004;  /*< ApiSystemNameData */
    public static final int             getKnownPeersSystemTime     = 2005;  /*< ApiPeerSystemTimeDataList */
    public static final int             markLicenseOverflow         = 2006;  /*< ApiLicenseOverflowData */
    public static final int             getSettings                 = 2007;  /*< ApiResourceParamDataList */
    
    public static final int             getHelp                     = 9003;  /*< ApiHelpGroupDataList */
    public static final int             runtimeInfoChanged          = 9004;  /*< ApiRuntimeData */
    public static final int             dumpDatabase                = 9005;  /*< ApiDatabaseDumpData */
    public static final int             restoreDatabase             = 9006;  /*< ApiDatabaseDumpData */
    public static final int             updatePersistentSequence    = 9009;  /*< ApiUpdateSequenceData*/

    public static String commandName(int command) {
        switch (command) {

        case saveResource:
            return "saveResource";
        case setResourceStatus:
            return "setResourceStatus";
        case setResourceParam:
            return "setResourceParam";
        case setResourceParams:
            return "setResourceParams";
        case removeResource:
            return "removeResource";

        case getCamerasEx:
            return "getCamerasEx";
        case saveCameras:
            return "saveCameras";
        case saveCamera:
            return "saveCamera";
        case saveCameraUserAttributes:
            return "saveCameraUserAttributes";            
        case removeCamera:
            return "removeCamera";            

        case getLayouts:
            return "getLayouts";
        case saveLayouts:
            return "saveLayouts";
        case saveLayout:
            return "saveLayout";            
        case removeLayout:
            return "removeLayout";            

        case getMediaServersEx:
            return "getMediaServersEx";            
        case saveMediaServer:
            return "saveMediaServer";
        case saveServerUserAttributes:
            return "saveServerUserAttributes";            
        case removeMediaServer:
            return "removeMediaServer";

        case getUsers:
            return "getUsers";            
        case removeUser:
            return "removeUser";
        case saveUser:
            return "saveUser";
            
        case getCameraHistoryItems:
            return "getCameraHistoryItems";
        case addCameraHistoryItem:
            return "addCameraHistoryItem";
            
        default:
            /* Make sure command list is in sync with QnTransactionTransport::skipTransactionForMobileClient method. */
            if (BuildConfig.DEBUG)
                throw new AssertionError("Invalid transaction "+ command);
            
            return "Unknown transaction " + command;
        }
    }

}
