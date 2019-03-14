#ifdef ENABLE_ONVIF

#ifndef dw_resource_settings_h_1957
#define dw_resource_settings_h_1957

#include <QtNetwork/QAuthenticator>

#include <core/resource/camera_advanced_param.h>

/*
* base class either for new and old DW settings
*/
class DWAbstractCameraProxy
{
public:
    DWAbstractCameraProxy(const QString& host, int port, unsigned int timeout,
        const QAuthenticator& auth);
    virtual ~DWAbstractCameraProxy() {}

    virtual QnCameraAdvancedParamValueList getParamsList() const = 0;
    virtual bool setParams(const QVector<QPair<QnCameraAdvancedParameter, QString>>& parameters,
        QnCameraAdvancedParamValueList* result = nullptr) = 0;
    virtual void setCameraAdvancedParams(const QnCameraAdvancedParams& params) = 0;
protected:
    const QString m_host;
    int m_port;
    unsigned int m_timeout;
    const QAuthenticator m_auth;
};

class QnWin4NetCameraProxy: public DWAbstractCameraProxy
{
public:
    QnWin4NetCameraProxy(const QString& host, int port, unsigned int timeout,
        const QAuthenticator& auth);

    virtual QnCameraAdvancedParamValueList getParamsList() const override;
    //virtual bool setParam(const QnCameraAdvancedParameter &parameter, const QString &value) override;
    virtual bool setParams(const QVector<QPair<QnCameraAdvancedParameter, QString>>& parameters,
        QnCameraAdvancedParamValueList* result = nullptr) override;
    virtual void setCameraAdvancedParams(const QnCameraAdvancedParams& params) override { m_params = params; };

private:
    QnCameraAdvancedParamValueList fetchParamsFromHttpResponse(const QByteArray& body) const;
    QnCameraAdvancedParamValueList requestParamValues(const QString& request) const;
    QString toInnerValue(const QnCameraAdvancedParameter& parameter, const QString& value) const;
    QString fromInnerValue(const QnCameraAdvancedParameter& parameter, const QString &value) const;
    bool setParam(const QnCameraAdvancedParameter& parameter, const QString& value);

private:
    QnCameraAdvancedParams m_params;
};

class QnPravisCameraProxy: public DWAbstractCameraProxy
{
public:
    QnPravisCameraProxy(const QString& host, int port, unsigned int timeout,
        const QAuthenticator& auth);

    virtual void setCameraAdvancedParams(const QnCameraAdvancedParams& params) override;

    virtual QnCameraAdvancedParamValueList getParamsList() const override;
    virtual bool setParams(const QVector<QPair<QnCameraAdvancedParameter, QString>>& parameters,
        QnCameraAdvancedParamValueList* result = nullptr) override;
private:
    void addToFlatParams(const QnCameraAdvancedParamGroup& group);
    QString toInnerValue(const QnCameraAdvancedParameter& parameter, const QString& value) const;
    QString fromInnerValue(const QnCameraAdvancedParameter& parameter, const QString& value) const;
private:
    QMap<QString, QnCameraAdvancedParameter> m_flatParams;
};

#endif //dw_resource_settings_h_1957

#endif  //ENABLE_ONVIF
