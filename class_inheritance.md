# Class Inheritance Trees

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## QnAbstractStreamDataProvider

```
QnLongRunnable (QThread)
    QnAbstractStreamDataProvider
        QnAbstractMediaStreamDataProvider
            QnAbstractArchiveStreamReader
                QnArchiveStreamReader
            AbstractLiveStreamProvider
                QnLiveStreamProvider (+ServerModuleAware)
                    CLServerPushStreamReader (+QnAbstractMediaStreamProvider)
                        QnRtpStreamReader
                        MJPEGStreamReader
                        PlDlinkStreamReader
                        ThirdPartyStreamReader
                        QnDesktopCameraStreamReader
                        ArecontPanoramicTftpStreamReader
                        ArecontTftpStreamReader
                        MultisensorDataProvider
                        QnTestCameraStreamReader
                        QnVMax480LiveProvider (+QnVmax480DataConsumer)
                        ProxyLiveStreamProvider
                        WebRtcCameraStreamReader
                    TimeStampedDataProvider  (test)
                    LiveStreamProviderMock   (test)
            QnThumbnailsStreamReader
            QnSingleShotFileStreamreader
        QnSpeechSynthesisDataProvider
        ProxyDataProvider
            PutInOrderDataProvider (+AbstractDataReorderer)
        DesktopDataProviderBase
            DesktopAudioOnlyDataProvider
```

---

## QnStreamRecorder

```
QnLongRunnable
    QnAbstractDataConsumer (+QnAbstractMediaDataReceptor)
        QnStreamRecorder (+nx::AbstractRecordingContext)
            ServerStreamRecorder
                ServerStorageStreamRecorder (+StorageRecordingContext)
                    QnServerEdgeStreamRecorder
                CloudStreamRecorder (+CloudRecordingContext)
            ExportStorageStreamRecorder (+nx::StorageRecordingContext)
            NovMediaExport (+nx::StorageRecordingContext)
```

---

## QnResource

```
QObject
    QnResource (+QnFromThisToShared<QnResource>)
        QnMediaResource
            QnAbstractArchiveResource
                QnAviResource
                    DesktopResource
                        DesktopAudioOnlyResource
            QnVirtualCameraResource (+EnableSafeDirectConnection)
                nx::vms::server::resource::Camera (+ServerModuleOnlyAware)
                    QnPlDlinkResource
                    QnPlIqResource
                    QnPlIsdResource
                    QnTestCameraResource
                    WebRtcCameraResource
                    QnPlVmax480Resource
                    FcResource  (Flir)
                    IoModuleResource
                    CameraMock  (test)
                nx::vms::client::core::CameraResource
                    CrossSystemCameraResource
                CameraResourceStub  (test)
        QnMediaServerResource (+EnableSafeDirectConnection)
            nx::vms::client::core::ServerResource
                CrossSystemServerResource
        QnAbstractStorageResource
            QnStorageResource
                nx::vms::server::resource::StorageResource
                    CloudStorageResource
                QnDbStorageResource
                QnExtIODeviceStorageResource
                QnLayoutFileStorageResource
                QnQtFileStorageResource
                QnClientStorageResource  (client)
                StorageResourceStub  (test)
        QnLayoutResource
            nx::vms::client::core::LayoutResource
                CrossSystemLayoutResource
                SnapshotLayoutResource  (internal)
        QnUserResource
            nx::vms::client::core::UserResource
        QnWebPageResource
        LocalAudioFileResource
        QnVideoWallResource
        nx::vms::common::AnalyticsPluginResource
        nx::vms::common::AnalyticsEngineResource
        DummyResource
```
