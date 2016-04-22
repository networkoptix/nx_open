#include "transaction.h"

#include <atomic>

#include "common/common_module.h"
#include "database/db_manager.h"
#include "utils/common/synctime.h"

#include "utils/common/model_functions.h"
#include "transaction_descriptor.h"

namespace ec2
{

    namespace ApiCommand
    {
        struct GetNameByValueDescriptorVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d) { result = d.name; }
            GetNameByValueDescriptorVisitor() : result(nullptr) {}

            const char *result;
        };

        QString toString(Value val)
        {
            GetNameByValueDescriptorVisitor visitor;
            visitTransactionDescriptorIfValue(val, visitor);

            if (visitor.result)
                return visitor.result;
            else
                return "unknown " + QString::number((int)val);
        }

        struct GetValueByNameDescriptorVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d) { result = d.tag; }

            GetValueByNameDescriptorVisitor() : result(ApiCommand::NotDefined) {}

            ApiCommand::Value result;
        };

        Value fromString(const QString& val)
        {
            GetValueByNameDescriptorVisitor visitor;
            visitTransactionDescriptorIfName(val.toLatin1().constData(), visitor);

            return visitor.result;
        }

        struct IsSystemVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d) { result = d.isSystem; }

            IsSystemVisitor() : result(false) {}

            bool result;
        };

        bool isSystem( Value val )
        {
            IsSystemVisitor visitor;
            visitTransactionDescriptorIfValue(val, visitor);
            return visitor.result;
        }

        struct IsPersistentVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d) { result = d.isPersistent; }

            IsPersistentVisitor() : result(false) {}

            bool result;
        };

        bool isPersistent( Value val )
        {
            IsPersistentVisitor visitor;
            visitTransactionDescriptorIfValue(val, visitor);
            return visitor.result;
        }

    }

    int generateRequestID()
    {
        static std::atomic<int> requestID( 0 );
        return ++requestID;
    }

    QString QnAbstractTransaction::toString() const
    {
        return lit("command=%1 time=%2 peer=%3 dbId=%4 dbSeq=%5")
            .arg(ApiCommand::toString(command))
            .arg(persistentInfo.timestamp)
            .arg(peerID.toString())
            .arg(persistentInfo.dbID.toString())
            .arg(persistentInfo.sequence);
    }

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction::PersistentInfo,    (json)(ubjson)(xml)(csv_record),   QnAbstractTransaction_PERSISTENT_Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAbstractTransaction,                    (json)(ubjson)(xml)(csv_record),   QnAbstractTransaction_Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiTransactionData,                    (json)(ubjson)(xml)(csv_record),   ApiTransactionDataFields)

} // namespace ec2

