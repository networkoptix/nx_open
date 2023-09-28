// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "evaluator.h"

#include <nx/utils/literal.h>
#include <nx/fusion/serialization/lexical_functions.h>

namespace Qee {
// -------------------------------------------------------------------------- //
// Utility
// -------------------------------------------------------------------------- //
    QString serialized(TokenType type) {
        switch (type) {
        case Variable:  return lit("VARIABLE");
        case Number:    return lit("NUMBER");
        case Plus:      return lit("PLUS");
        case Minus:     return lit("MINUS");
        case Times:     return lit("TIMES");
        case Divide:    return lit("DIVIDE");
        case BitAnd:    return lit("BITAND");
        case BitOr:     return lit("BITOR");
        case BitNot:    return lit("BITNOT");
        case LParen:    return lit("LPAREN");
        case RParen:    return lit("RPAREN");
        case Dot:       return lit("DOT");
        case Comma:     return lit("COMMA");
        case End:       return lit("END");
        case Invalid:   return lit("INVALID");
        default:        NX_ASSERT(false); return QString();
        }
    }

    QString serialized(InstructionType type) {
        switch (type) {
        case Stor:      return lit("STOR");
        case Add:       return lit("ADD");
        case Sub:       return lit("SUB");
        case Mul:       return lit("MUL");
        case Div:       return lit("DIV");
        case Neg:       return lit("NEG");
        case Udd:       return lit("UDD");
        case And:       return lit("AND");
        case Or:        return lit("OR");
        case Not:       return lit("NOT");
        case Call:      return lit("CALL");
        case MCall:     return lit("MCALL");
        case Nop:       return lit("NOP");
        default:        NX_ASSERT(false); return QString();
        }
    }


// -------------------------------------------------------------------------- //
// Lexer
// -------------------------------------------------------------------------- //
    Token Lexer::readNextToken() {
        if(m_token.type() != Invalid) {
            Token result = m_token;
            m_token = Token();
            return result;
        }

        while(true) {
            QChar c = m_source[m_pos];
            switch(c.unicode()) {
            case L' ': case L'\t': case L'\n': case L'\r':
                m_pos++;
                break;
            case L'_':
                return readVariableToken();
            case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': case L'8': case L'9':
                return readNumberToken();
            case L'#':
                return readColorToken();
            case L'-':
                return readSymbolToken(Minus);
            case L'+':
                return readSymbolToken(Plus);
            case L'*':
                return readSymbolToken(Times);
            case L'/':
                return readSymbolToken(Divide);
            case L'&':
                return readSymbolToken(BitAnd);
            case L'|':
                return readSymbolToken(BitOr);
            case L'~':
                return readSymbolToken(BitNot);
            case L'(':
                return readSymbolToken(LParen);
            case L')':
                return readSymbolToken(RParen);
            case L'.':
                return readSymbolToken(Dot);
            case L',':
                return readSymbolToken(Comma);
            case L'\0':
                return readSymbolToken(End);
            default:
                if(c.isLetter()) {
                    return readVariableToken();
                } else {
                    unexpected();
                }
            }
        }
    }

    Token Lexer::peekNextToken() {
        if(m_token.type() == Invalid)
            m_token = readNextToken();
        return m_token;
    }

    void Lexer::unexpected() const {
        throw Exception(lit("Unexpected symbol '%1' at position %2.").arg(m_source[m_pos]).arg(m_pos));
    }

    Token Lexer::readNumberToken() {
        int startPos = m_pos;
        while(true) {
            switch(m_source[m_pos].unicode()) {
            case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': case L'8': case L'9':
                m_pos++;
                break;
            default:
                return Token(Number, QStringView(m_source).mid(startPos, m_pos - startPos), startPos);
            }
        }
    }

    Token Lexer::readColorToken() {
        int startPos = m_pos;
        m_pos++; /* Skip '#' */
        while(true) {
            switch(m_source[m_pos].unicode()) {
            case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': case L'8': case L'9':
            case L'a': case L'b': case L'c': case L'd': case L'e': case L'f':
            case L'A': case L'B': case L'C': case L'D': case L'E': case L'F':
                m_pos++;
                break;
            default:
                return Token(Color, QStringView(m_source).mid(startPos, m_pos - startPos), startPos);
            }
        }
    }

    Token Lexer::readVariableToken() {
        int startPos = m_pos;
        while(true) {
            QChar c = m_source[m_pos];
            if(c.isLetterOrNumber() || c.unicode() == L'_') {
                m_pos++;
                continue;
            } else {
                return Token(Variable, QStringView(m_source).mid(startPos, m_pos - startPos), startPos);
            }
        }
    }

    Token Lexer::readSymbolToken(TokenType type) {
        m_pos++;
        return Token(type, QStringView(m_source).mid(m_pos - 1, 1), m_pos - 1);
    }


// -------------------------------------------------------------------------- //
// Parser
// -------------------------------------------------------------------------- //
    QVector<Instruction> Parser::parse() {
        m_program.clear();
        parseExpr();

        Token token = m_lexer->peekNextToken();
        if(token.type() != End)
            unexpected(token);

        return m_program;
    }

    void Parser::unexpected(const Token &token) const {
        throw Exception(lit("Unexpected token %1 ('%2') at position %3.").arg(serialized(token.type())).arg(token.text().toString()).arg(token.pos()));
    }

    void Parser::require(TokenType type) {
        Token token = m_lexer->readNextToken();
        if(token.type() != type)
            unexpected(token);
    }

    void Parser::parseArgs(int *argc) {
        /* args ::= '('')' | '(' expr {',' expr} ')' | */
        *argc = 0;

        Token token = m_lexer->peekNextToken();
        if(token.type() != LParen)
            return;

        require(LParen);

        token = m_lexer->peekNextToken();
        if(token.type() == RParen) {
            require(RParen);
            return;
        }

        *argc = 1;
        parseExpr();

        while(true) {
            token = m_lexer->peekNextToken();
            if(token.type() == Comma) {
                require(Comma);
                parseExpr();
                (*argc)++;
            } else {
                require(RParen);
                return;
            }
        }
    }

    void Parser::parseInvocation() {
        /* invocation ::= COLOR | VAR args */
        Token token = m_lexer->peekNextToken();
        switch (token.type()) {
        case Variable: {
            require(Variable);
            int argc;
            parseArgs(&argc);
            m_program.push_back(Instruction(Stor, token.text().toString()));
            m_program.push_back(Instruction(Call, argc));
            break;
        }
        case Color: {
            require(Color);
            QColor color;
            if(!QnLexical::deserialize(token.text().toString(), &color))
                throw Exception(lit("Invalid color constant '%1'.").arg(token.text().toString()));
            m_program.push_back(Instruction(Stor, color));
            break;
        }
        default:
            unexpected(token);
            break;
        }
    }

    void Parser::parseChain() {
        /* chain ::= invocation {'.' invocation} */
        Token token = m_lexer->peekNextToken();
        switch (token.type()) {
        case Variable:
        case Color:
            parseInvocation();
            break;
        default:
            unexpected(token);
            break;
        }

        while(true) {
            token = m_lexer->peekNextToken();
            if(token.type() == Dot) {
                require(Dot);
                parseInvocation();

                /* Replace Call with MCall */
                m_program.back().setType(MCall);
            } else {
                return;
            }
        }
    }

    void Parser::parseFactor() {
        /* factor ::= chain | INT | '(' expr ')' | ('-' | '+' | '~') factor */
        Token token = m_lexer->peekNextToken();
        switch(token.type()) {
        case Number: {
            require(Number);
            long long number;
            if(!QnLexical::deserialize(token.text().toString(), &number))
                throw Exception(lit("Invalid number constant '%1'.").arg(token.text().toString()));
            m_program.push_back(Instruction(Stor, number));
            break;
        }

        case Color:
        case Variable:
            parseChain();
            break;

        case LParen:
            require(LParen);
            parseExpr();
            require(RParen);
            break;

        case Minus:
            require(Minus);
            parseFactor();
            m_program.push_back(Instruction(Neg));
            break;

        case Plus:
            require(Plus);
            parseFactor();
            m_program.push_back(Instruction(Udd));
            break;

        case BitNot:
            require(BitNot);
            parseFactor();
            m_program.push_back(Instruction(Neg));
            break;

        default:
            unexpected(token);
        }
    }

    void Parser::parseTerm() {
        /* term ::= factor {('*' | '/') factor} */
        Token token = m_lexer->peekNextToken();
        switch(token.type()) {
        case Number:
        case Variable:
        case Color:
        case LParen:
        case Minus:
        case Plus:
        case BitNot:
            parseFactor();
            break;

        default:
            unexpected(token);
        }

        while(true) {
            token = m_lexer->peekNextToken();
            switch(token.type()) {
            case Times:
            case Divide:
                require(token.type());
                parseFactor();
                m_program.push_back(Instruction(token.type() == Times ? Mul : Div));
                break;
            default:
                return;
            }
        }
    }

    void Parser::parseBitFactor() {
        /* bitfactor ::= term {('+' | '-') term} */
        Token token = m_lexer->peekNextToken();
        switch(token.type()) {
        case Number:
        case Variable:
        case Color:
        case LParen:
        case Minus:
        case Plus:
        case BitNot:
            parseTerm();
            break;

        default:
            unexpected(token);
        }

        while(true) {
            token = m_lexer->peekNextToken();
            switch(token.type()) {
            case Plus:
            case Minus:
                require(token.type());
                parseTerm();
                m_program.push_back(Instruction(token.type() == Plus ? Add : Sub));
                break;
            default:
                return;
            }
        }
    }

    void Parser::parseBitTerm() {
        /* bitterm ::= bitfactor {'&' bitfactor} */
        Token token = m_lexer->peekNextToken();
        switch(token.type()) {
        case Number:
        case Variable:
        case Color:
        case LParen:
        case Minus:
        case Plus:
        case BitNot:
            parseBitFactor();
            break;

        default:
            unexpected(token);
        }

        while(true) {
            token = m_lexer->peekNextToken();
            if(token.type() == BitAnd) {
                require(token.type());
                parseBitFactor();
                m_program.push_back(Instruction(And));
            } else {
                return;
            }
        }
    }

    void Parser::parseExpr() {
        /* expr ::= bitterm {'|' bitterm} */
        Token token = m_lexer->peekNextToken();
        switch(token.type()) {
        case Number:
        case Variable:
        case Color:
        case LParen:
        case Minus:
        case Plus:
        case BitNot:
            parseBitTerm();
            break;

        default:
            unexpected(token);
        }

        while(true) {
            token = m_lexer->peekNextToken();
            if(token.type() == BitOr) {
                require(token.type());
                parseBitTerm();
                m_program.push_back(Instruction(Or));
            } else {
                return;
            }
        }
    }


// -------------------------------------------------------------------------- //
// Evaluator Functions and Constants
// -------------------------------------------------------------------------- //
    QVariant eval_QColor(const ParameterPack &args) {
        if (args.size() == 0)
            return QColor();

        args.requireSize(3, 4);
        int r = args.get<int>(0);
        int g = args.get<int>(1);
        int b = args.get<int>(2);
        int a = 255;
        if(args.size() == 4)
            a = args.get<int>(3);

        return QColor(r, g, b, a);
    }

    QVariant eval_QColor_lighter(const ParameterPack &args) {
        args.requireSize(1, 2);

        if(args.size() == 1) {
            return args.get<QColor>(0).lighter();
        } else {
            return args.get<QColor>(0).lighter(args.get<int>(1));
        }
    }

    QVariant eval_QColor_darker(const ParameterPack &args) {
        args.requireSize(1, 2);

        if(args.size() == 1) {
            return args.get<QColor>(0).darker();
        } else {
            return args.get<QColor>(0).darker(args.get<int>(1));
        }
    }

    QVariant eval_QColor_setRed(const ParameterPack &args) {
        args.requireSize(2);

        QColor result = args.get<QColor>(0);
        result.setRed(args.get<int>(1));
        return result;
    }

    QVariant eval_QColor_setGreen(const ParameterPack &args) {
        args.requireSize(2);

        QColor result = args.get<QColor>(0);
        result.setGreen(args.get<int>(1));
        return result;
    }

    QVariant eval_QColor_setBlue(const ParameterPack &args) {
        args.requireSize(2);

        QColor result = args.get<QColor>(0);
        result.setBlue(args.get<int>(1));
        return result;
    }

    QVariant eval_QColor_setAlpha(const ParameterPack &args) {
        args.requireSize(2);

        QColor result = args.get<QColor>(0);
        result.setAlpha(args.get<int>(1));
        return result;
    }

// -------------------------------------------------------------------------- //
// Evaluator
// -------------------------------------------------------------------------- //
    Evaluator::Evaluator() {
        m_functionTypeId = qMetaTypeId<Function>();
        m_resolver = nullptr;
    }

    QVariant Evaluator::constant(const QString &name) const {
        return m_constants.value(name);
    }

    void Evaluator::registerConstant(const QString &name, const QVariant &value) {
        m_constants.insert(name, value);
    }

    void Evaluator::registerFunction(const QString &name, const Function &function) {
        m_constants.insert(name, QVariant::fromValue(function));
    }

    void Evaluator::registerFunctions()
    {
        registerFunction("QColor", &eval_QColor);
        registerFunction("QColor::setAlpha", &eval_QColor_setAlpha);
    }

    QVariant Evaluator::evaluate(const Program &program) const {
        QVector<QVariant> stack;

        for(const Instruction &instruction: program)
            exec(stack, instruction);

        if(stack.size() != 1)
            throw Exception(lit("Invalid stack size after program evaluation: %1.").arg(program.size()));

        return stack[0];
    }

    void Evaluator::exec(QVector<QVariant> &stack, const Instruction &instruction) const {
        switch(instruction.type()) {
        case Stor:
            stor(stack, instruction.data());
            break;
        case Add:
        case Sub:
        case Mul:
        case Div:
        case Or:
        case And:
            binop(stack, instruction.type());
            break;
        case Neg:
        case Udd:
        case Not:
            unop(stack, instruction.type());
            break;
        case Call:
        case MCall:
            call(stack, instruction);
            break;
        default:
            NX_ASSERT(false);
        }
    }

    void Evaluator::stor(QVector<QVariant> &stack, const QVariant &data) const {
        stack.push_back(data);
    }

    void Evaluator::binop(QVector<QVariant> &stack, InstructionType op) const {
        QVariant r = pop_back(stack);
        QVariant l = pop_back(stack);

        int type = superType(r.userType(), l.userType());
        if(type == QMetaType::UnknownType)
            throw Exception(lit("Could not deduce result type for operation %1('%2', '%3').").arg(serialized(op)).arg(QLatin1String(l.typeName())).arg(QLatin1String(r.typeName())));

        if(type == QMetaType::LongLong) {
            stack.push_back(binop(l.toLongLong(), r.toLongLong(), op));
        } else {
            stack.push_back(binop(l.toDouble(), r.toDouble(), op));
        }
    }

    long long Evaluator::binop(long long l, long long r, InstructionType op) const {
        switch (op) {
        case Add:   return l + r;
        case Sub:   return l - r;
        case Mul:   return l * r;
        case Div:   return l / r;
        case And:   return l & r;
        case Or:    return l | r;
        default:    NX_ASSERT(false); return 0;
        }
    }

    double Evaluator::binop(double l, double r, InstructionType op) const {
        switch (op) {
        case Add:   return l + r;
        case Sub:   return l - r;
        case Mul:   return l * r;
        case Div:   return l / r;
        case And:
        case Or:    throw IllegalArgumentException(lit("Invalid parameter type for operation %1('%2', '%2')").arg(serialized(op)).arg(lit("double")));
        default:    NX_ASSERT(false); return 0;
        }
    }

    void Evaluator::unop(QVector<QVariant> &stack, InstructionType op) const {
        QVariant a = pop_back(stack);

        int type = upperType(a.userType());
        if(type == QMetaType::UnknownType)
            throw Exception(lit("Could not deduce arithmetic supertype for type '%1'.").arg(QLatin1String(a.typeName())));

        if(type == QMetaType::LongLong) {
            stack.push_back(unop(a.toLongLong(), op));
        } else {
            stack.push_back(unop(a.toDouble(), op));
        }
    }

    long long Evaluator::unop(long long v, InstructionType op) const {
        switch (op) {
        case Neg:   return -v;
        case Udd:   return v;
        case Not:   return ~v;
        default:    NX_ASSERT(false); return 0;
        }
    }

    double Evaluator::unop(double v, InstructionType op) const {
        switch (op) {
        case Neg:   return -v;
        case Udd:   return v;
        case Not:   throw IllegalArgumentException(lit("Invalid parameter type for operation %1('%2')").arg(serialized(op)).arg(lit("double")));
        default:    NX_ASSERT(false); return 0;
        }
    }

    void Evaluator::call(QVector<QVariant> &stack, const Instruction &instruction) const {
        QVariant argcVariant = instruction.data();
        if(argcVariant.userType() != QMetaType::Int)
            throw Exception(lit("Argument number for %1 instruction has invalid type '%2'.").arg(serialized(instruction.type())).arg(QLatin1String(argcVariant.typeName())));
        int argc = argcVariant.toInt();

        if(argc < 0)
            throw Exception(lit("Argument number for %1 instruction is invalid.").arg(serialized(instruction.type())));
        if(instruction.type() == MCall)
            argc++; /* `this` is treated as another argument. */

        if(stack.size() < argc + 1)
            throw Exception(lit("Stack underflow during execution of %1 instruction.").arg(serialized(instruction.type())));

        QVariant nameVariant = stack[stack.size() - 1];
        if(nameVariant.userType() != QMetaType::QString)
            throw Exception(lit("Function name for %1 instruction has invalid type '%2'.").arg(serialized(instruction.type())).arg(QLatin1String(nameVariant.typeName())));
        QString name = nameVariant.toString();

        if(instruction.type() == MCall)
            name = QLatin1String(stack[stack.size() - argc - 1].typeName()) + lit("::") + name;

        QVariant variable = m_constants.value(name);
        if(variable.userType() == QMetaType::UnknownType) {
            if(m_resolver)
                variable = m_resolver->resolveConstant(name);
            if(variable.userType() == QMetaType::UnknownType)
                throw Exception(lit("Function or variable '%1' is not defined.").arg(name));
        }

        QVariant result;
        if(variable.userType() != m_functionTypeId) {
            if(argc > 0)
                throw Exception(lit("Variable '%1' is not a function and cannot be called.").arg(name));
            result = variable;
        } else {
            result = variable.value<Function>()(ParameterPack(stack, argc, name));
        }

        stack.resize(stack.size() - argc - 1);
        stack.push_back(result);
    }

    QVariant Evaluator::pop_back(QVector<QVariant> &stack) {
        if(stack.isEmpty())
            throw Exception(lit("Stack underflow during program evaluation."));

        QVariant result = stack.back();
        stack.pop_back();
        return result;
    }

    int Evaluator::superType(int l, int r) {
        int ul = upperType(l);
        int ur = upperType(r);

        if(ul == QMetaType::UnknownType || ur == QMetaType::UnknownType)
            return QMetaType::UnknownType;

        if(ul == QMetaType::Double || ur == QMetaType::Double)
            return QMetaType::Double;

        return QMetaType::LongLong;
    }

    int Evaluator::upperType(int type) {
        switch (type) {
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::UChar:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::ULong:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            return QMetaType::LongLong;
        case QMetaType::Float:
        case QMetaType::Double:
            return QMetaType::Double;
        default:
            return QMetaType::UnknownType;
        }
    }

} // namespace Qee
