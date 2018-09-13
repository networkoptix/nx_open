#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

// TODO: #Elric doesn't really belong in this folder
struct QnMediaDewarpingParams
{
    Q_GADGET
    Q_ENUMS(ViewMode)

    Q_PROPERTY(bool enabled MEMBER enabled)
    Q_PROPERTY(ViewMode viewMode MEMBER viewMode)
    Q_PROPERTY(qreal fovRot MEMBER fovRot)
    Q_PROPERTY(qreal xCenter MEMBER xCenter)
    Q_PROPERTY(qreal yCenter MEMBER yCenter)
    Q_PROPERTY(qreal radius MEMBER radius)
    Q_PROPERTY(qreal hStretch MEMBER hStretch)

public:
    enum ViewMode
    {
        Horizontal,
        VerticalDown,
        VerticalUp
    };

    friend bool operator==(const QnMediaDewarpingParams &l, const QnMediaDewarpingParams &r);
    friend bool operator!=(const QnMediaDewarpingParams &l, const QnMediaDewarpingParams &r);
    friend QDebug &operator<<(QDebug &stream, const QnMediaDewarpingParams &params);

    /**
     * Compatibility function to support reading data from previous software version.
     */
    static QnMediaDewarpingParams deserialized(const QByteArray &data);

    /** List of all possible panoFactor values for the resource with this dewarping parameters. */
    const QList<int>& allowedPanoFactorValues() const;

    /** List of all possible panoFactor values for the selected view mode. */
    static const QList<int>& allowedPanoFactorValues(ViewMode mode);

public:
    /** Whether dewarping is currently enabled. */
    bool enabled = false;

    /** Camera mounting mode. */
    ViewMode viewMode = VerticalDown;

    /** View correction angle, in degrees. */
    qreal fovRot = 0.0;

    /** Normalized position of the circle*/
    qreal xCenter = 0.5;
    qreal yCenter = 0.5;

    /** Circle radius in range 0..1 (r/width) */
    qreal radius = 0.5;

    /** Horizontal stretch. Value 1.0 means no stretch */
    qreal hStretch = 1.0;
};

#define QnMediaDewarpingParams_Fields (enabled)(viewMode)(fovRot)(xCenter)(yCenter)(radius)(hStretch)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnMediaDewarpingParams::ViewMode)

QN_FUSION_DECLARE_FUNCTIONS(QnMediaDewarpingParams::ViewMode, (lexical)(numeric))

QN_FUSION_DECLARE_FUNCTIONS(QnMediaDewarpingParams, (json)(metatype))
