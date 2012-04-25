#ifndef axis_resource_h_2215
#define axis_resource_h_2215

#include <QMap>
#include <QSharedPointer>
#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlAxisResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlAxisResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    virtual bool hasDualStreaming() const override { return true; }

    bool isInitialized() const;

    QByteArray getMaxResolution() const;
    QString getNearestResolution(const QByteArray& resolution, float aspectRatio) const;
    float getResolutionAspectRatio(const QByteArray& resolution) const;

    QRect getMotionWindow(int num) const;
    QMap<int, QRect>  getMotionWindows() const;
    void readMotionInfo();

    void updateCameraMotion(const QnMotionRegion& region);
protected:
    void init();
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
private:
    void clear();
    static QRect axisRectToGridRect(const QRect& axisRect);
    static QRect gridRectToAxisRect(const QRect& gridRect);

    bool removeMotionWindow(int wndNum);
    int addMotionWindow();
    bool updateMotionWindow(int wndNum, int sensitivity, const QRect& rect);
    int toAxisMotionSensitivity(int sensitivity);
private:
    QList<QByteArray> m_resolutionList;
    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;
    qint64 m_lastMotionReadTime;
};

typedef QSharedPointer<QnPlAxisResource> QnPlAxisResourcePtr;

#endif //axis_resource_h_2215
