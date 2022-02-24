// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <core/resource/resource_fwd.h>
#include <plugins/resource/desktop_camera/desktop_data_provider_wrapper.h>
#include <plugins/resource/desktop_camera/desktop_camera_connection.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

class QnDesktopDataProvider;
class QOpenGLWidget;

class QnWinDesktopResource: public QnDesktopResource
{
public:
    QnWinDesktopResource(QOpenGLWidget* mainWindow = nullptr);
    virtual ~QnWinDesktopResource();

    virtual bool isRendererSlow() const override;
    virtual AudioLayoutConstPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

private:
    QnAbstractStreamDataProvider* createDataProviderInternal();

    void createSharedDataProvider();

    friend class QnDesktopDataProviderWrapper;

private:
    QOpenGLWidget* const m_mainWidget;
    QnDesktopDataProvider* m_desktopDataProvider = nullptr;
    nx::Mutex m_dpMutex;
};
