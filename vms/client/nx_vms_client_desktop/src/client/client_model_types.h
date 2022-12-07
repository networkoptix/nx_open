// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDataStream>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QWeakPointer>
#include <QtGui/QVector3D>

#include <client/client_globals.h>
#include <core/resource/layout_item_data.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <recording/time_period.h>
#include <utils/common/id.h>

// -------------------------------------------------------------------------- //
// QnThumbnailsSearchState
// -------------------------------------------------------------------------- //
/**
 * Additional data for a thumbnails search layout.
 */
struct QnThumbnailsSearchState {
    QnThumbnailsSearchState(): step(0) {}
    QnThumbnailsSearchState(const QnTimePeriod &period, qint64 step): period(period), step(step) {}

    QnTimePeriod period;
    qint64 step;
};
Q_DECLARE_METATYPE(QnThumbnailsSearchState)

// -------------------------------------------------------------------------- //
// QnWorkbenchState
// -------------------------------------------------------------------------- //
/**
 * Serialized workbench state. Does not contain the actual layout data, so
 * suitable for restoring the state once layouts are loaded.
 */
class QnWorkbenchState
{
public:
    struct UnsavedLayout
    {
        QnUuid id;
        QnUuid parentId;
        QString name;
        qreal cellSpacing = 0.0;
        float cellAspectRatio = 0.0;
        QString backgroundImageFilename;
        qreal backgroundOpacity = 0.0;
        QSize backgroundSize;
        QnLayoutItemDataList items;
        bool isCrossSystem = false;
    };

    QnWorkbenchState();
    QnUuid userId;          /*< Id of the user. Works as first part of the key. */
    QnUuid localSystemId;   /*< Id of the system. Works as second part of the key. */
    QnUuid currentLayoutId;
    QnUuid runningTourId;
    QList<QnUuid> layoutUuids;
    QList<UnsavedLayout> unsavedLayouts;

    bool isValid() const;
};

#define QnWorkbenchStateUnsavedLayoutItem_Fields (resource)(uuid)(flags)(combinedGeometry) \
    (zoomTargetUuid)(zoomRect)(rotation)(displayInfo)(controlPtz)(displayAnalyticsObjects) \
    (displayRoi)(frameDistinctionColor)(contrastParams)(dewarpingParams)
#define QnWorkbenchStateUnsavedLayout_Fields (id)(parentId)(name)(cellSpacing)(cellAspectRatio) \
    (backgroundImageFilename)(backgroundOpacity)(backgroundSize)(items)(isCrossSystem)
#define QnWorkbenchState_Fields \
    (userId)(localSystemId)(currentLayoutId)(runningTourId)(layoutUuids)

using QnWorkbenchStateList = QList<QnWorkbenchState>;

QN_FUSION_DECLARE_FUNCTIONS(QnWorkbenchState, (json)(metatype))
QN_FUSION_DECLARE_FUNCTIONS(QnWorkbenchState::UnsavedLayout, (json))

// -------------------------------------------------------------------------- //
// QnLicenseWarningState
// -------------------------------------------------------------------------- //
struct QnLicenseWarningState {
    QnLicenseWarningState(qint64 lastWarningTime = 0): lastWarningTime(lastWarningTime) {}

    qint64 lastWarningTime;
};

/**
 * Mapping from license key to license warning state.
 */
typedef QHash<QByteArray, QnLicenseWarningState> QnLicenseWarningStateHash;

Q_DECLARE_METATYPE(QnLicenseWarningStateHash);
QN_FUSION_DECLARE_FUNCTIONS(QnLicenseWarningState, (datastream)(metatype));

// -------------------------------------------------------------------------- //
// QnBackgroundImage
// -------------------------------------------------------------------------- //
/**
 * Set of options how to display client background.
 */
struct QnBackgroundImage
{
    bool enabled = false;
    QString name;
    QString originalName;
    Qn::ImageBehavior mode = Qn::ImageBehavior::Crop;
    qreal opacity = 0.5;

    qreal actualImageOpacity() const;

    static QnBackgroundImage defaultBackground();

    bool operator==(const QnBackgroundImage& other) const;
};
#define QnBackgroundImage_Fields (enabled)(name)(originalName)(mode)(opacity)

QN_FUSION_DECLARE_FUNCTIONS(QnBackgroundImage, (json)(metatype));
NX_REFLECTION_INSTRUMENT(QnBackgroundImage, QnBackgroundImage_Fields)
