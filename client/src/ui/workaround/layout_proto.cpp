#include "layout_proto.h"

#include <utils/serialization/proto_message.h>

#include <nx_ec/data/api_layout_data.h>

namespace QnLayoutProto {

    namespace Resource {
        enum ResourceField {
            IdField = 2,
            TypeIdField = 3,
            ParentIdField = 4,
            NameField = 5,
            UrlField = 6,
            StatusField = 7,
            DisabledField = 8,
            UuidField = 9,
            PropertyField = 10,
            LayoutField = 103
        };
    }

    namespace Layout {
        enum LayoutField {
            CellAspectRatioField = 1,
            CellSpacingWidthField = 2,
            CellSpacingHeightField = 3,
            ItemField = 4,
            UserCanEditField = 5,
            LockedField = 6,
            BackgroundImageFilenameField = 7,
            BackgroundWidthField = 8,
            BackgroundHeightField = 9,
            BackgroundOpacityField = 10
        };
    }

    namespace LayoutItem {
        enum LayoutItemField {
            UuidField = 1,
            FlagsField = 2,
            LeftField = 3,
            TopField = 4,
            RightField = 5,
            BottomField = 6,
            RotationField = 7,
            ResourceField = 8,
            ZoomLeftField = 9,
            ZoomTopField = 10,
            ZoomRightField = 11,
            ZoomBottomField = 12,
            ZoomTargetUuidField = 13,
            ContrastParamsField  = 14,
            DewarpingParamsField = 15
        };
    }

    namespace LayoutItemResource {
        enum LayoutItemResourceField {
            IdField = 1,
            PathField = 2
        };
    }

    bool deserialize(const QnProtoValue &value, QnProtoMessage *target) {
        if(value.type() != QnProtoValue::Bytes)
            return false;

        QnProtoParseError error;
        *target = QnProtoMessage::fromProto(value.toBytes(), &error);
        return error.error == QnProtoParseError::NoError;
    }

    bool deserialize(const QnProtoValue &value, QByteArray *target) {
        if(value.type() != QnProtoValue::Bytes)
            return false;

        *target = value.toBytes();
        return true;
    }

    bool deserialize(const QnProtoValue &value, QString *target) {
        if(value.type() != QnProtoValue::Bytes)
            return false;

        *target = QString::fromUtf8(value.toBytes());
        return true;
    }

    bool deserialize(const QnProtoValue &value, QUuid *target) {
        if(value.type() != QnProtoValue::Bytes)
            return false;

        *target = QUuid(value.toBytes()); /* We don't check for error here because we don't care. */
        return true;
    }

    bool deserialize(const QnProtoValue &value, double *target) {
        if(value.type() != QnProtoValue::Fixed64)
            return false;

        *target = value.toDouble();
        return true;
    }

    bool deserialize(const QnProtoValue &value, float *target) {
        if(value.type() == QnProtoValue::Fixed64) {
            *target = value.toDouble();
        } else if(value.type() == QnProtoValue::Fixed32) {
            *target = value.toFloat();
        } else {
            return false;
        }
        
        return true;
    }

    bool deserialize(const QnProtoValue &value, bool *target) {
        if(value.type() != QnProtoValue::Variant)
            return false;

        *target = value.toBool();
        return true;
    }

    bool deserialize(const QnProtoValue &value, qint32 *target) {
        if(value.type() != QnProtoValue::Variant && value.type() != QnProtoValue::Fixed32)
            return false;

        *target = value.toInt32();
        return true;
    }

    bool deserialize(const QnProtoValue &value, ec2::ApiLayoutItemData *target) {
        QnProtoMessage message;
        if(!deserialize(value, &message))
            return false;

        for(const QnProtoRecord &record: message) {
            switch(record.index()) {
            case LayoutItem::UuidField:
                if(!deserialize(record.value(), &target->id))
                    return false;
                break;
            case LayoutItem::FlagsField:
                if(!deserialize(record.value(), &target->flags))
                    return false;
                break;
            case LayoutItem::LeftField:
                if(!deserialize(record.value(), &target->left))
                    return false;
                break;
            case LayoutItem::TopField:
                if(!deserialize(record.value(), &target->top))
                    return false;
                break;
            case LayoutItem::RightField:
                if(!deserialize(record.value(), &target->right))
                    return false;
                break;
            case LayoutItem::BottomField:
                if(!deserialize(record.value(), &target->bottom))
                    return false;
                break;
            case LayoutItem::RotationField:
                if(!deserialize(record.value(), &target->rotation))
                    return false;
                break;
            case LayoutItem::ResourceField: {
                QnProtoMessage message;
                if(!deserialize(record.value(), &message))
                    return false;

                for(const QnProtoRecord &record: message) {
                    switch(record.index()) {
                    case LayoutItemResource::PathField:
                        if(!deserialize(record.value(), &target->resourcePath))
                            return false;
                        break;
                    default:
                        break; /* Ignore other fields. */
                    }
                }                
                
                break;
            }
            case LayoutItem::ZoomLeftField:
                if(!deserialize(record.value(), &target->zoomLeft))
                    return false;
                break;
            case LayoutItem::ZoomTopField:
                if(!deserialize(record.value(), &target->zoomTop))
                    return false;
                break;
            case LayoutItem::ZoomRightField:
                if(!deserialize(record.value(), &target->zoomRight))
                    return false;
                break;
            case LayoutItem::ZoomBottomField:
                if(!deserialize(record.value(), &target->zoomBottom))
                    return false;
                break;
            case LayoutItem::ZoomTargetUuidField:
                if(!deserialize(record.value(), &target->zoomTargetId))
                    return false;
                break;
            case LayoutItem::ContrastParamsField:
                if(!deserialize(record.value(), &target->contrastParams))
                    return false;
                break;
            case LayoutItem::DewarpingParamsField:
                if(!deserialize(record.value(), &target->dewarpingParams))
                    return false;
                break;
            default:
                break; /* Ignore other fields. */
            }
        }

        return true;
    }

    bool deserialize(const QnProtoValue &value, ec2::ApiLayoutData *target) {
        QnProtoMessage message;
        if(!deserialize(value, &message))
            return false;

        for(const QnProtoRecord &record: message) {
            switch(record.index()) {
            case Resource::NameField:
                if(!deserialize(record.value(), &target->name))
                    return false;
                break;
            case Resource::UuidField:
                if(!deserialize(record.value(), &target->id))
                    return false;
                break;
            case Resource::LayoutField: {
                QnProtoMessage message;
                if(!deserialize(record.value(), &message))
                    return false;

                for(const QnProtoRecord &record: message) {
                    switch(record.index()) {
                    case Layout::CellAspectRatioField:
                        if(!deserialize(record.value(), &target->cellAspectRatio))
                            return false;
                        break;
                    case Layout::CellSpacingWidthField:
                        if(!deserialize(record.value(), &target->horizontalSpacing))
                            return false;
                        break;
                    case Layout::CellSpacingHeightField:
                        if(!deserialize(record.value(), &target->verticalSpacing))
                            return false;
                        break;
                    case Layout::ItemField: 
                        target->items.push_back(ec2::ApiLayoutItemData());
                        if(!deserialize(record.value(), &target->items.back()))
                            return false;
                        break;
                    case Layout::UserCanEditField:
                        if(!deserialize(record.value(), &target->editable))
                            return false;
                        break;
                    case Layout::LockedField:
                        if(!deserialize(record.value(), &target->locked))
                            return false;
                        break;
                    case Layout::BackgroundImageFilenameField:
                        if(!deserialize(record.value(), &target->backgroundImageFilename))
                            return false;
                        break;
                    case Layout::BackgroundWidthField:
                        if(!deserialize(record.value(), &target->backgroundWidth))
                            return false;
                        break;
                    case Layout::BackgroundHeightField:
                        if(!deserialize(record.value(), &target->backgroundHeight))
                            return false;
                        break;
                    case Layout::BackgroundOpacityField:
                        if(!deserialize(record.value(), &target->backgroundOpacity))
                            return false;
                        break;
                    default:
                        break; /* Ignore other fields. */
                    }
                }
                
                break;
            }
            default:
                break; /* Ignore other fields. */
            }
        }

        return true;
    }

    template<class T>
    bool deserialize(const QnProtoValue &value, QnProto::Message<T> *target) {
        QnProtoMessage message;
        if(!deserialize(value, &message))
            return false;

        for(const QnProtoRecord &record: message)
            if(record.index() == 1)
                return deserialize(record.value(), &target->data);

        return false;
    }

    template<class T>
    bool deserialize(const QByteArray &value, T *target) {
        return deserialize(QnProtoValue(value), target);
    }


} // namespace QnLayoutProto

namespace QnProto {

    bool deserialize(const QByteArray &value, Message<ec2::ApiLayoutData> *target) {
        return QnLayoutProto::deserialize(value, target);
    }

    bool deserialize(const QnProtoValue &value, Message<ec2::ApiLayoutData> *target) {
        return QnLayoutProto::deserialize(value, target);
    }

} // namespace QnProto

