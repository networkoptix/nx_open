#ifndef QN_EVALUATOR_H
#define QN_EVALUATOR_H

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QCoreApplication>

#include "exception.h"

namespace Qee {
    enum TokenType {
        Variable,
        Number,
        Color,
        Plus,
        Minus,
        Times,
        Divide,
        BitAnd,
        BitOr,
        BitNot,
        LParen,
        RParen,
        Dot,
        Comma,
        End,

        Invalid = -1
    };

    enum InstructionType {
        Stor,
        Add,
        Sub,
        Mul,
        Div,
        Neg,
        Udd,
        And,
        Or,
        Not,
        Call,
        MCall,
        Nop = -1
    };

    enum StandardFunction {
        ColorFunctions = 0x1,
        ColorNames = 0x2
    };
    Q_DECLARE_FLAGS(StandardFunctions, StandardFunction)
    Q_DECLARE_OPERATORS_FOR_FLAGS(StandardFunctions)

    QString serialized(TokenType type);

    inline void serialize(TokenType type, QString *target) {
        *target = serialized(type);
    }

    QString serialized(InstructionType type);

    inline void serialize(InstructionType type, QString *target) {
        *target = serialized(type);
    }

    class Exception: public QnException {
    public:
        Exception(const QString &message = QString()): QnException(message) {}

        // TODO: #Elric add stack
    };

    class NullPointerException: public Exception {
    public:
        NullPointerException(const QString &message = QString()): Exception(message) {}
    };

    class IllegalArgumentException: public Exception {
    public:
        IllegalArgumentException(const QString &message = QString()): Exception(message) {}
    };


    class Token {
    public:
        Token(): m_type(Invalid), m_pos(-1) {}
        Token(TokenType type, const QStringRef &text, int pos): m_type(type), m_text(text), m_pos(pos) {}
        
        TokenType type() const { return m_type; }
        QStringRef text() const { return m_text; }
        int pos() const { return m_pos; }

    private:
        TokenType m_type;
        QStringRef m_text;
        int m_pos;
    };


    class Instruction {
    public:
        explicit Instruction(InstructionType type = Nop, QVariant data = QVariant()): m_type(type), m_data(data) {}

        InstructionType type() const { return m_type; }
        void setType(InstructionType type) { m_type = type; }

        const QVariant &data() const { return m_data; }
        void setData(const QVariant &data) { m_data = data; }

    private:
        InstructionType m_type;
        QVariant m_data;
    };


    class Lexer {
        Q_DECLARE_TR_FUNCTIONS(Qee::Lexer)
    public:
        Lexer(const QString &source): m_source(source + QChar(L'\0')), m_pos(0) {}

        Token readNextToken();
        Token peekNextToken();

    private:
        void unexpected() const;

        Token readNumberToken();
        Token readColorToken();
        Token readVariableToken();
        Token readSymbolToken(TokenType type);

    private:
        QString m_source;
        int m_pos;
        Token m_token;
    };


    typedef QVector<Instruction> Program;


    class Parser {
        Q_DECLARE_TR_FUNCTIONS(Qee::Parser)
    public:
        Parser(Lexer *lexer): m_lexer(lexer) {};

        Program parse();

        static Program parse(const QString &source) {
            Lexer lexer(source);
            Parser parser(&lexer);
            return parser.parse();
        }

    private:
        void unexpected(const Token &token) const;

        void require(TokenType type);
        void parseArgs(int *argc);
        void parseInvocation();
        void parseChain();
        void parseFactor();
        void parseTerm();
        void parseBitFactor();
        void parseBitTerm();
        void parseExpr();

    private:
        Lexer *m_lexer;
        Program m_program;
    };


    class ParameterPack {
        Q_DECLARE_TR_FUNCTIONS(Qee::ParameterPack)
    public:
        ParameterPack(const QVector<QVariant> &stack, int size, const QString &name): m_stack(stack), m_size(size), m_name(name) {}

        int size() const { return m_size; }

        const QString &name() { return m_name; }

        QVariant get(int index) const { 
            if(index < 0 || index >= m_size)
                throw IllegalArgumentException(tr("Parameter %2 is not specified for function '%1'.").arg(m_name).arg(index));
            return ref(index);
        }

        template<class T>
        T get(int index) const {
            if(index < 0 || index >= m_size)
                throw IllegalArgumentException(tr("Parameter %2 is not specified for function '%1'.").arg(m_name).arg(index));
            const QVariant &param = ref(index);
            if(!param.canConvert<T>())
                throw IllegalArgumentException(tr("Parameter %2 of function '%1' is of type '%3', but type '%4' was expected.").arg(m_name).arg(index).arg(QLatin1String(param.typeName())).arg(QLatin1String(QMetaType::typeName(qMetaTypeId<T>()))));
            return param.value<T>();
        }

        void requireSize(int size) const {
            if(m_size != size)
                throw IllegalArgumentException(tr("Function '%1' is expected to have %3 arguments, %2 provided.").arg(m_name).arg(m_size).arg(size));
        }

        void requireSize(int minSize, int maxSize) const {
            if(m_size < minSize || m_size > maxSize)
                throw IllegalArgumentException(tr("Function '%1' is expected to have %3-%4 arguments, %2 provided.").arg(m_name).arg(m_size).arg(minSize).arg(maxSize));
        }

    private:
        const QVariant &ref(int index) const {
            return m_stack[m_stack.size() - m_size - 1 + index];
        }

    private:
        const QVector<QVariant> &m_stack;
        int m_size;
        QString m_name;
    };

    typedef QVariant (*Function)(const ParameterPack &); 

    
    class Resolver {
    public:
        virtual ~Resolver() {}
        virtual QVariant resolveConstant(const QString &name) const = 0;
    };


    class Evaluator {
        Q_DECLARE_TR_FUNCTIONS(Qee::Evaluator)
    public:
        Evaluator();

        QVariant evaluate(const Program &program) const;

        Resolver *resolver() const { return m_resolver; }
        void setResolver(Resolver *resolver) { m_resolver = resolver; }

        QVariant constant(const QString &name) const;
        void registerConstant(const QString &name, const QVariant &value);
        void registerFunction(const QString &name, const Function &function);
        void registerFunctions(StandardFunctions functions);

    private:
        void exec(QVector<QVariant> &stack, const Instruction &instruction) const;
        void stor(QVector<QVariant> &stack, const QVariant &data) const;
        void binop(QVector<QVariant> &stack, InstructionType op) const;
        long long binop(long long l, long long r, InstructionType op) const;
        double binop(double l, double r, InstructionType op) const;
        void unop(QVector<QVariant> &stack, InstructionType op) const;
        long long unop(long long v, InstructionType op) const;
        double unop(double v, InstructionType op) const;
        void call(QVector<QVariant> &stack, const Instruction &instruction) const;
        
        static QVariant pop_back(QVector<QVariant> &stack);
        static int superType(int l, int r);
        static int upperType(int type);

    private:
        int m_functionTypeId;
        QHash<QString, QVariant> m_constants;
        Resolver *m_resolver;
    };

} // namespace Qee

Q_DECLARE_METATYPE(Qee::Function)

#endif // QN_EVALUATOR_H

