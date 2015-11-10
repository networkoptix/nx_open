#include "onvif_imaging_settings.h"

#ifdef ENABLE_ONVIF

#include "onvif/soapStub.h"

//
// class QnAbstractOnvifImagingOperation
// 
QnAbstractOnvifImagingOperation::QnAbstractOnvifImagingOperation(ImagingSettingsResp* settings):
    m_settings(settings)
{}

QnAbstractOnvifImagingOperation::~QnAbstractOnvifImagingOperation() {}

//
// class QnFloatOnvifImagingOperation
// 
QnFloatOnvifImagingOperation::QnFloatOnvifImagingOperation(ImagingSettingsResp* settings, pathFunction path):
    QnAbstractOnvifImagingOperation(settings),
    m_path(path)
{}

QnFloatOnvifImagingOperation::~QnFloatOnvifImagingOperation() {}

bool QnFloatOnvifImagingOperation::get(QString& value) const {
    if (!m_settings->ImagingSettings || !m_path(m_settings))
        return false;
    value = QString::number(*(m_path(m_settings)));
    return true;
}

bool QnFloatOnvifImagingOperation::set(const QString& value) const {
    if (!m_settings->ImagingSettings || !m_path(m_settings))
        return false;

    bool ok;
    float result = value.toFloat(&ok);
    if (!ok)
        return false;
    *(m_path(m_settings)) = result;
    return true;
}

//
// class QnEnumOnvifImagingOperation
// 
QnEnumOnvifImagingOperation::QnEnumOnvifImagingOperation(ImagingSettingsResp* settings, const QStringList &range, pathFunction path):
    QnAbstractOnvifImagingOperation(settings),
    m_path(path),
    m_range(range) 
{}

QnEnumOnvifImagingOperation::~QnEnumOnvifImagingOperation() {}

bool QnEnumOnvifImagingOperation::get(QString& value) const {
    if (!m_settings->ImagingSettings || !m_path(m_settings))
        return false;
    int idx = (*(m_path(m_settings)));
    if (idx < 0 || idx >= m_range.size())
        return false;
    value = m_range[idx];
    return true;
}

bool QnEnumOnvifImagingOperation::set(const QString& value) const {
    if (!m_settings->ImagingSettings || !m_path(m_settings))
        return false;
    int idx = m_range.indexOf(value);
    if (idx < 0)
        return false;
    *(m_path(m_settings)) = idx;
    return true;
}

#endif
