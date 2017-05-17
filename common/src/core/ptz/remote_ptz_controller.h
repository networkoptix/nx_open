#pragma once

#include <QtCore/QAtomicInt>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

class QnRemotePtzController: public QnAbstractPtzController {
    Q_OBJECT
    typedef QnAbstractPtzController base_type;

public:
    QnRemotePtzController(const QnNetworkResourcePtr &resource);
    virtual ~QnRemotePtzController();

    virtual Ptz::Capabilities getCapabilities() const override;

    virtual bool continuousMove(const QVector3D& speed) override;
    virtual bool continuousFocus(qreal speed) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const QVector3D& position,
        qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;
    virtual bool getFlip(Qt::Orientations* flip) const override;

    virtual bool createPreset(const QnPtzPreset& preset) override;
    virtual bool updatePreset(const QnPtzPreset& preset) override;
    virtual bool removePreset(const QString& presetId) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList* presets) const override;

    virtual bool createTour(const QnPtzTour& tour) override;
    virtual bool removeTour(const QString& tourId) override;
    virtual bool activateTour(const QString& tourId) override;
    virtual bool getTours(QnPtzTourList* tours) const override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;
    virtual bool updateHomeObject(const QnPtzObject& homeObject) override;
    virtual bool getHomeObject(QnPtzObject* homeObject) const override;

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const override;
    virtual bool runAuxilaryCommand(
        const QnPtzAuxilaryTrait& trait,
        const QString& data) override;

    virtual bool getData(Qn::PtzDataFields query, QnPtzData* data) const override;

private slots:
    void at_replyReceived(int status, const QVariant& reply, int handle);

private:
    bool isPointless(Qn::PtzCommand command);

    int nextSequenceNumber();
    QnMediaServerResourcePtr getMediaServer() const;

private:
    // TODO: #ynikitenkov move to private
    struct PtzCommandData
    {
        PtzCommandData():
            command(Qn::InvalidPtzCommand)
        {
        }

        PtzCommandData(Qn::PtzCommand command, const QVariant& value):
            command(command),
            value(value)
        {
        }

        Qn::PtzCommand command;
        QVariant value;
    };

    QnNetworkResourcePtr m_resource; //TODO: #GDM check if it is a camera
    QnUuid m_sequenceId;
    QAtomicInt m_sequenceNumber;

    mutable QnMutex m_mutex;
    QHash<int, PtzCommandData> m_dataByHandle;
};

