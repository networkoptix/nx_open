// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>

#include <core/ptz/proxy_ptz_controller.h>

#include <utils/common/from_this_to_shared.h>

class QnCachingPtzController:
    public QnProxyPtzController,
    public QnFromThisToShared<QnCachingPtzController>
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

public:
    QnCachingPtzController(const QnPtzControllerPtr& baseController);
    virtual ~QnCachingPtzController();

    virtual void initialize() override;

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool getLimits(
        QnPtzLimits* limits,
        CoordinateSpace space,
        const Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const Options& options) const override;

    virtual bool getPresets(QnPtzPresetList* presets) const override;

    virtual bool getTours(QnPtzTourList* tours) const override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;

    virtual bool getHomeObject(QnPtzObject* homeObject) const override;

    virtual bool getAuxiliaryTraits(
        QnPtzAuxiliaryTraitList* auxiliaryTraits,
        const Options& options) const override;

    virtual bool getData(
        QnPtzData* data,
        DataFields query,
        const Options& options) const override;

protected:
    virtual void baseFinished(Command command, const QVariant& data) override;

private:
    bool initializeInternal();

    template<class T>
    DataFields updateCacheLocked(
        DataField field,
        T QnPtzData::*member,
        const T& value);

    template<class T>
    DataFields updateCacheLocked(
        DataField field,
        T QnPtzData::*member,
        const QVariant& value);

    DataFields updateCacheLocked(const QnPtzData& data);

private:
    bool m_initialized;
    mutable nx::Mutex m_mutex;
    QnPtzData m_data;
};
