#include <sstream>
#include <thread>

#include "timer.h"
#include "tx_device.h"
#include "mpeg_ts_packet.h"
#include "rx_device.h"
#include "camera_manager.h"
#include "discovery_manager.h"

#if defined(_DEBUG)
// Logger logger("/opt/networkoptix/mediaserver/var/log/it930x.log");
#endif


namespace ite {
namespace aux {

static int minStrength()
{
    static const int kDefaultMinStrength = 70;
    return settings.minStrength < 0 ? kDefaultMinStrength : settings.minStrength;
}

} // namespace aux

//	std::mutex It930x::m_rcMutex;
RxSyncManager rxSyncManager;
RxHintManager rxHintManager;

unsigned str2num(std::string& s)
{
    unsigned value;
    std::stringstream ss;
    ss << s;
    ss >> value;
    return value;
}

std::string num2str(unsigned id)
{
    std::stringstream ss;
    ss << id;
    return ss.str();
}

//
RxDevice::~RxDevice()
{
    ITE_LOG() << FMT("Stopping Rx %d", m_rxID);
    pleaseStop();
    if (m_watchDogThread.joinable()) {
        m_watchDogThread.join();
    }
    close();
    ITE_LOG() << FMT("Stopping Rx %d Done", m_rxID);
}

RxDevice::RxDevice(unsigned id)
  : m_deviceReady(false),
    m_needStop(false),
    m_running(false),
    m_configuring(false),
    m_devReader(new DevReader),
    m_rxID(id),
    m_channel(NOT_A_CHANNEL),
    m_signalQuality(0),
    m_signalStrength(0),
    m_signalPresent(false),
    m_cam(nullptr)
{
}

bool RxDevice::good() const
{
    return present() && strength() > aux::minStrength();
}

// <refactor>
void RxDevice::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_txDev) {
        rxSyncManager.unlock(m_txDev->txID());
    }
    if (!m_cam)
        debug_printf("[RxDevice::shutdown] Rx %d. No related camera found\n!", m_rxID);
    else
    {
        std::lock_guard<std::mutex> lock(m_cam->get_mutex());
        m_cam->rxDeviceRef().reset();
        m_cam = nullptr;
    }

    stopReader();
    m_txDev.reset();
    m_it930x->unlockFrequency();

    ITE_LOG() << FMT("Rx %d shutdown done", m_rxID);
    m_deviceReady = false;
}

void RxDevice::startWatchDog()
{
    // starting watchdog thread
    static const int WATCHDOG_GOOD_TIMEOUT = 3000;      // ms
    static const int WATCHDOG_BAD_TIMEOUT = 3 * 1000;  // ms

    m_watchDogThread = nx::utils::thread(
        [this]
        {
            if(open())
            {
                m_running = true;
                ITE_LOG() << FMT("Open rx %d successfull", m_rxID);
            }
            else
            {
                ITE_LOG() << FMT("Open rx %d failed, device is not operational", m_rxID);
                return;
            }

            int timeout = WATCHDOG_GOOD_TIMEOUT;
            while (true)
            {
                if (m_needStop)
                    break;

                stats();
                ITE_LOG() << FMT("[Rx watchdog] Rx %d getting stats DONE; " \
                                 "stats: %d; m_deviceReady: %d; m_needStop: %d",
                                 m_rxID, (int)good(), (int)m_deviceReady.load(), (int)m_needStop);

                if (!good() || !m_deviceReady)
                {
                    debug_printf("[Rx watchdog] Rx %d became bad. shutting down.\n", m_rxID);
                    shutdown();

                    if(findBestTx())
                        timeout = WATCHDOG_GOOD_TIMEOUT;
                    else
                        timeout = WATCHDOG_BAD_TIMEOUT;
                }
                else if (!m_devReader->hasThread())
                {
                    printf("[Rx watchdog] Rx %d NO READER thread. shutting down.\n", m_rxID);
                    shutdown();

                    if(findBestTx())
                        timeout = WATCHDOG_GOOD_TIMEOUT;
                    else
                        timeout = WATCHDOG_BAD_TIMEOUT;
                }
                else if (good() && m_deviceReady)
                {
                    Timer testTimer;
                    const int TEST_TIME = 4000;
                    bool first = true;
                    bool wrongCamera = false;

                    debug_printf("[Rx watchdog] Rx %d. Device seems good. Starting testing camera ID\n", m_rxID);

                    m_devReader->subscribe(It930x::PID_RETURN_CHANNEL);

                    while (testTimer.elapsedMS() < TEST_TIME)
                    {
                        RcPacketBuffer pktBuf;
                        if (! m_devReader->getRcPacket(pktBuf))
                        {
                            debug_printf("[Rx watchdog] Rx %d. NO RC packet\n", m_rxID);
                            break;
                        }

                        if (pktBuf.pid() != It930x::PID_RETURN_CHANNEL)
                        {
                            debug_printf("[Rx watchdog] packet error: wrong PID\n");
                            continue;
                        }

                        if (pktBuf.checkbit())
                        {
                            debug_printf("[Rx watchdog] packet error: TS error bit\n");
                            continue;
                        }

                        RcPacket pkt = pktBuf.packet();
                        if (! pkt.isOK())
                        {
                            pkt.print();
                            continue;
                        }

                        if (pkt.txID() == 0 || pkt.txID() == 0xffff)
                        {
                            debug_printf("[Rx watchdog] wrong txID: %d\n", pkt.txID());
                            continue;
                        }

                        if (first)
                        {
                            if (pkt.txID() != m_cam->cameraId())
                            {
                                debug_printf(
                                    "[Rx watchdog] WRONG CAMERA. Pkt::TXID: %d, Cam::txID: %d shutting down\n",
                                    pkt.txID(),
                                    m_cam->cameraId()
                                );
                                wrongCamera = true;
                                break;
                            }
                        }
                    }

                    m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);

                    if (wrongCamera)
                    {
                        shutdown();

                        if(findBestTx())
                            timeout = WATCHDOG_GOOD_TIMEOUT;
                        else
                            timeout = WATCHDOG_BAD_TIMEOUT;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            }
            m_running = false;
        }
    );
}


bool RxDevice::testChannel(int chan, int &bestChan, int &bestStrength, int &txID)
{
    // We need to read from channel for some time
    static const int SEARCH_READ_TIME = 2000; // ms

    m_it930x->unlockFrequency();
    if (m_it930x->lockFrequency(TxDevice::freq4chan(chan)))
    {
        ITE_LOG() << FMT("[search] Rx: %d; lock channel %d succeded getting stats", m_rxID, chan);
        stats();

        if (good())
        {
            ITE_LOG() << FMT("[search] Rx: %d; get stats %d succeded, starting reading", m_rxID, chan);

            if (m_devReader)
                m_devReader->stop();
            else
                m_devReader.reset(new DevReader);

            m_devReader->subscribe(It930x::PID_RETURN_CHANNEL);
            m_devReader->start(m_it930x.get(), SEARCH_READ_TIME);

            ITE_LOG() << FMT("[search] Rx: %d; channel %d waiting for the reader", m_rxID, chan);

            m_devReader->wait();
            ITE_LOG() << FMT("[search] Rx: %d; channel %d wait for read is over", m_rxID, chan);

            bool first = true;
            for (;;)
            {
                if (m_needStop)
                    return false;

                ITE_LOG() << FMT("[search] Rx: %d, channel: %d, reading RC channel", m_rxID, chan);

                RcPacketBuffer pktBuf;
                if (! m_devReader->getRcPacket(pktBuf))
                {
                    ITE_LOG() << FMT("[search] Rx %d. NO RC packet", m_rxID);
                    break;
                }

                if (pktBuf.pid() != It930x::PID_RETURN_CHANNEL)
                {
                    ITE_LOG() << FMT("[RC] packet error: wrong PID");
                    break;
                }

                if (pktBuf.checkbit())
                {
                    ITE_LOG() << FMT("[RC] packet error: TS error bit");
                    break;
                }

                RcPacket pkt = pktBuf.packet();
                if (! pkt.isOK())
                {
                    pkt.print();
                    break;
                }

                if (pkt.txID() == 0 || pkt.txID() == 0xffff)
                {
                    ITE_LOG() << FMT("[RC] wrong txID: %d\n", pkt.txID());
                    break;
                }

                ITE_LOG() << FMT("[RC] Rx %d pkt.txID: %d", m_rxID, pkt.txID());

                if (first)
                {
                    bestChan = chan;
                    bestStrength = strength();
                    m_channel = chan;
                    txID = pkt.txID();

                    m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);
                    m_it930x->unlockFrequency();
                    return true;
                }
            }
            m_devReader->unsubscribe(It930x::PID_RETURN_CHANNEL);
        }
    }
    m_it930x->unlockFrequency();
    return false;
}

bool RxDevice::lockAndStart(int chan, int txId)
{
    m_it930x->lockFrequency(TxDevice::freq4chan(chan));
    m_txDev.reset(new TxDevice(txId, TxDevice::freq4chan(chan)));
    printf("[search] Rx %d; Found camera %d FINAL\n", m_rxID, txId);
    for (int i = 0; i < 2; ++i) {
        subscribe(i);
    }
    startReader();
    m_deviceReady.store(true);
    return true;
}

bool RxDevice::findBestTx()
{
    int bestStrength = 0;
    int bestChan = 0;
    int txID = 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    // trying hints first
    auto hints = rxHintManager.getHintsByRx(m_rxID);

    for (const auto &hint: hints)
    {
        if (m_needStop)
            return false;

        if (rxHintManager.useHint(m_rxID, hint.txId, hint.chan))
        {
            if (testChannel(hint.chan, bestChan, bestStrength, txID) &&
                txID == hint.txId &&
                rxSyncManager.lock(m_rxID, hint.txId))
            {
                ITE_LOG() << FMT("[search] Hint (rxID: %d, chan: %d, txID: %d) helped", m_rxID, hint.chan, hint.txId);
                return lockAndStart(bestChan, txID);
            }
            else
            {
                ITE_LOG() << FMT("[search] Hint (rxID: %d, hint.chan: %d, hint.txID: %d) failed; actual TxID: %d", m_rxID, hint.chan, hint.txId, txID);
            }
        }
    }

    // hints didn't help, starting normal discovery routine
    for (int i = 0; i < TxDevice::CHANNELS_NUM; ++i)
    {
        if (m_needStop)
            return false;

        if (testChannel(i, bestChan, bestStrength, txID))
        {
            ITE_LOG() << FMT("[search] Rx: %d. Test channel %d succeeded. Strength is %d, camera is %d\n", m_rxID, i, bestStrength, txID);

            if (rxSyncManager.lock(m_rxID, txID))
                return lockAndStart(bestChan, txID);
            else
            {
                ITE_LOG() << FMT("[search] Rx: %d. This channel (%d) seems already locked. Need to continue.\n", m_rxID, i);
            }

        }
//            else
//                ITE_LOG() << FMT("[search] Rx: %d. testChannel() failed for channel %d", m_rxID, i);
    }

    m_it930x->unlockFrequency();
    printf("[search] Rx: %d; Search failed\n", m_rxID);

    return false;
}

// If there is no camera connected returns empty camera info,
// otherwise - return actual info.
nxcip::CameraInfo RxDevice::getCameraInfo() const
{
    debug_printf("[RxDevice::getCameraInfo] Rx %d; Asked for camera info\n", m_rxID);

    nxcip::CameraInfo info;
    memset(&info, 0, sizeof(nxcip::CameraInfo));
    if (!m_deviceReady || !m_txDev)
    {
        debug_printf(
            "[RxDevice::getCameraInfo] Rx %d; Can't get camera info camera info\n",
            m_rxID
        );
        return info;
    }

    std::string strTxID = RxDevice::id2str(m_txDev->txID());
    std::string url = std::string("localhost:") + strTxID;

    strncpy( info.modelName, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::modelName)-1) ); // TODO
    strncpy( info.url, url.c_str(), sizeof(nxcip::CameraInfo::url)-1 );
    strncpy( info.uid, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::uid)-1) );

    std::stringstream ss;
    ss << m_rxID << ',' <<  m_txDev->txID() << ',' << TxDevice::freq4chan(m_channel);

    unsigned len = std::min(ss.str().size(), sizeof(nxcip::CameraInfo::auxiliaryData)-1);
    strncpy(info.auxiliaryData, ss.str().c_str(), len);
    info.auxiliaryData[len] = 0;

    return info;
}
// </refactor

bool RxDevice::open()
{
    try
    {
        m_it930x.reset(new It930x(m_rxID));
        m_it930x->info(m_rxInfo);
        return true;
    }
    catch (DtvException& ex)
    {
        debug_printf("[it930x] %s %d\n", ex.what(), ex.code());
        m_it930x.reset();
    }

    return false;
}

void RxDevice::close()
{
    stopReader();
    m_it930x.reset();
}

void RxDevice::resetFrozen()
{
    try
    {
        if (m_it930x)
        {
            m_it930x->rxReset();
        }
    }
    catch (DtvException& ex)
    {
        debug_printf("[it930x] %s %d\n", ex.what(), ex.code());

        m_it930x.reset();
    }
}

bool RxDevice::isLocked() const
{
    std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

    return isLocked_u();
}

void RxDevice::stats()
{
    if (!m_it930x)
        return;

    m_signalQuality = 0;
    m_signalStrength = 0;
    m_signalPresent = false;
    bool locked = false;
    m_it930x->statistic(m_signalQuality, m_signalStrength, m_signalPresent, locked);
}

void RxDevice::processRcQueue()
{
    TxDevicePtr txDev = m_txDev;
    if (! txDev)
        return;

    RcPacketBuffer pktBuf;
    while (m_devReader->getRcPacket(pktBuf))
    {
        RcPacket pkt = pktBuf.packet();
        if (! pkt.isOK())
        {
            debug_printf("[RC] bad packet\n");
            continue;
        }

        txDev->parse(pkt);
    }
}

bool RxDevice::configureTx()
{
    static const unsigned WAIT_MS = 30000;

    TxDevicePtr txDev = m_txDev;
    if (! txDev)
        return false;
    if (txDev->ready())
        return true;

    Timer timer(true);
    while (! txDev->ready())
    {
        if (timer.elapsedMS() > WAIT_MS)
        {
            debug_printf("[camera] break config process (timeout). Tx: %d; Rx: %d\n", txDev->txID(), rxID());
            break;
        }

        processRcQueue();
        updateTxParams();

        Timer::sleep(100);
    }

    return txDev->ready();
}

void RxDevice::updateTxParams()
{
    TxDevicePtr txDev = m_txDev;
    if (! txDev)
        return;

    uint16_t id = txDev->cmd2update();
    if (id)
    {
        RcCommand * pcmd = txDev->mkRcCmd(id);
        if (pcmd && pcmd->isValid())
            sendRC(pcmd);
    }
    else
        txDev->setReady();
}

bool RxDevice::sendRC(RcCommand * cmd)
{
    TxDevicePtr txDev = m_txDev;

    bool ok = cmd && cmd->isValid() && txDev && m_it930x;
    if (!ok)
    {
        debug_printf("[RC send] bad command or device. RxID: %d TxID: %d \
                      cmd: %p cmd->isValid(): %d m_it930x: %d\n",
                     rxID(), (txDev ? txDev->txID() : 0), cmd, (cmd ? cmd->isValid() : 0), (bool)m_it930x);
        Timer::sleep(RC_DELAY_MS());
        return false;
    }

    uint16_t cmdID = cmd->commandID();

    if (! txDev->setWanted(cmdID))
    {
        debug_printf("[RC send] not sent - waiting response. RxID: %d TxID: %d cmd: 0x%x\n", rxID(), txDev->txID(), cmdID);
        Timer::sleep(RC_DELAY_MS());
        return false;
    }

    SendSequence& sseq = txDev->sendSequence();

    std::lock_guard<std::mutex> lock(sseq.mutex); // LOCK

    std::vector<RcPacketBuffer> pkts;
    cmd->mkPackets(sseq, m_rxID, pkts);

    try
    {
        /// @warning possible troubles with 1-port devices if multipacket
        for (auto itPkt = pkts.begin(); itPkt != pkts.end(); ++itPkt)
        {
            m_it930x->sendRcPacket(itPkt->packet());
            debug_printf("[RC send] OK. RxID: %d TxID: %d cmd: 0x%x\n", rxID(), txDev->txID(), cmdID);
        }
    }
    catch (DtvException& ex)
    {
        debug_printf("[RC send][it930x] %s %d\n", ex.what(), ex.code());
    }

    return true;
}

bool RxDevice::sendRC(RcCommand * cmd, unsigned attempts)
{
    for (unsigned i = 0; i < attempts; ++i)
    {
        if (sendRC(cmd)) // delay inside
            return true;
    }

    return false;
}

//

void RxDevice::updateOSD()
{
    TxDevicePtr txDev = m_txDev;

    if (txDev && txDev->ready())
    {
        RcCommand * pcmd = txDev->mkSetOSD(0);
        sendRC(pcmd, RC_ATTEMPTS());

        pcmd = txDev->mkSetOSD(1);
        sendRC(pcmd, RC_ATTEMPTS());

        pcmd = txDev->mkSetOSD(2);
        sendRC(pcmd, RC_ATTEMPTS());
    }
}

bool RxDevice::changeChannel(unsigned chan)
{
    TxDevicePtr txDev = m_txDev;
    if (txDev && txDev->ready())
    {
        RcCommand * pcmd = txDev->mkSetChannel(chan);
        return sendRC(pcmd, RC_ATTEMPTS());
    }

    return false;
}

bool RxDevice::setEncoderParams(unsigned streamNo, const nxcip::LiveStreamConfig& config)
{
    TxDevicePtr txDev = m_txDev;
    if (txDev && txDev->ready())
    {
        RcCommand * pcmd = txDev->mkSetEncoderParams(streamNo, config.framerate, config.bitrateKbps);
        return sendRC(pcmd, RC_ATTEMPTS());
    }

    return false;
}

//

bool RxDevice::getEncoderParams(unsigned streamNo, nxcip::LiveStreamConfig& config)
{
    TxDevicePtr txDev = m_txDev;
    if (txDev && txDev->ready())
        return txDev->videoEncoderCfg(streamNo, config);
    return false;
}

//

void RxDevice::resolutionPassive(unsigned stream, int& width, int& height, float& fps)
{
    uint16_t pid = stream2pid(stream);

    fps = 30.0f;
    switch (pid)
    {
    case Pacidal::PID_VIDEO_FHD:
        width = 1920;
        height = 1080;
        break;

    case Pacidal::PID_VIDEO_HD:
        width = 1280;
        height = 720;
        break;

    case Pacidal::PID_VIDEO_SD:
        width = 640;
        height = 360;
        break;

    case Pacidal::PID_VIDEO_CIF:
        width = 0;
        height = 0;
        break;
    }
}

void RxDevice::resolution(unsigned stream, int& width, int& height, float& fps)
{
    TxDevicePtr txDev = m_txDev;
    if (!txDev || !txDev->ready() || !txDev->videoSourceCfg(stream, width, height, fps))
        resolutionPassive(stream, width, height, fps);
}

unsigned RxDevice::streamsCount()
{
    TxDevicePtr txDev = m_txDev;
    if (txDev && txDev->ready())
        return txDev->encodersCount();
    return 0;
}

//

unsigned RxDevice::dev2id(const std::string& nm)
{
    std::string name = nm;
    name.erase( name.find( DEVICE_PATTERN ), strlen( DEVICE_PATTERN ) );

    return str2id(name);
}

unsigned RxDevice::str2id(const std::string& name)
{
    unsigned num;
    std::stringstream ss;
    ss << name;
    ss >> num;
    return num;
}

std::string RxDevice::id2str(unsigned id)
{
    return num2str(id);
}

} // namespace ite
