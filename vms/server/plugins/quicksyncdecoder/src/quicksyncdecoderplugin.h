////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef QUICKSYNCDECODERPLUGIN_H
#define QUICKSYNCDECODERPLUGIN_H

#include <memory>

#include <D3D9.h>
#include <Dxva2api.h>

#include <mfxvideo++.h>

#include <QMutex>

#include <common/config.h>

#include <plugins/videodecoders/abstractvideodecoderusagecalculator.h>
#include <plugins/videodecoders/pluginusagewatcher.h>
#include <plugins/videodecoders/stree/resourcenameset.h>
#include <decoders/abstractvideodecoderplugin.h>

#include "d3dgraphicsadapterdescription.h"
#include "intelcpudescription.h"


//!Plugin of Intel Media SDK (Quicksync) based h.264 decoder
class QuicksyncDecoderPlugin
:
    public QObject,
    public QnAbstractVideoDecoderPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.networkoptix.plugin.videodecoder" FILE "quicksync_plugin.json")
    Q_INTERFACES(QnAbstractVideoDecoderPlugin)

public:
    QuicksyncDecoderPlugin();
    virtual ~QuicksyncDecoderPlugin();

    //!Implementation of QnAbstractClientPlugin::minSupportedVersion
    virtual quint32 minSupportedVersion() const;
    //!Implementation of QnAbstractClientPlugin::initializeLog
    virtual void initializeLog( QnLog* logInstance );
    //!Implementation of QnAbstractClientPlugin::initialized
    virtual bool initialized() const;

    //!Implementation of QnAbstractVideoDecoderPlugin::supportedCodecTypes
    virtual QList<AVCodecID> supportedCodecTypes() const;
    //!Implementation of QnAbstractVideoDecoderPlugin::isHardwareAccelerated
    virtual bool isHardwareAccelerated() const;
    //!Implementation of QnAbstractVideoDecoderPlugin::create
    virtual QnAbstractVideoDecoder* create(
        AVCodecID codecID,
        const QnCompressedVideoDataPtr& data,
        const QGLContext* const glContext,
        int currentSWDecoderCount ) const;
    //!Implementation of QnAbstractVideoDecoderPlugin::isStreamSupported
    virtual bool isStreamSupported( const nx::utils::stree::AbstractResourceReader& newStreamParams ) const override;

private:
    class D3D9DeviceContext
    {
    public:
        IDirect3DDevice9Ex* d3d9Device;
        IDirect3DDeviceManager9* d3d9manager;
        UINT deviceResetToken;

        D3D9DeviceContext()
        :
            d3d9Device( NULL ),
            d3d9manager( NULL ),
            deviceResetToken( 0 )
        {
        }
    };

    DecoderResourcesNameset m_resNameset;
    mutable std::unique_ptr<PluginUsageWatcher> m_usageWatcher;
    mutable std::unique_ptr<AbstractVideoDecoderUsageCalculator> m_usageCalculator;
    mutable std::unique_ptr<MFXVideoSession> m_mfxSession;
    mutable std::unique_ptr<D3DGraphicsAdapterDescription> m_graphicsDesc;
    mutable std::unique_ptr<IntelCPUDescription> m_cpuDesc;
    //!Graphics adapter number, quicksync is present on
    mutable unsigned int m_adapterNumber;
    mutable bool m_hardwareAccelerationEnabled;
    mutable IDirect3D9Ex* m_direct3D9;
    mutable QString m_sdkVersionStr;
    mutable bool m_initialized;
    QString m_osName;
    mutable HRESULT m_prevD3DOperationResult;
    mutable QMutex m_mutex;
    mutable std::vector<D3D9DeviceContext> m_d3dDevices;

    void initializeResourceNameset();
    bool initialize() const;
    bool openD3D9Device() const;
    void closeD3D9Device();
    QString winVersionToName( const OSVERSIONINFOEX& osVersionInfo ) const;
    bool isStreamSupportedNonSafe( const nx::utils::stree::AbstractResourceReader& newStreamParams ) const;
};

#endif  //QUICKSYNCDECODERPLUGIN_H
