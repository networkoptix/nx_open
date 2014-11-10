#include "evaluator.h"

#include <utils/serialization/lexical_functions.h>

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
        default:        assert(false); return QString();
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
        default:        assert(false); return QString();
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
        throw Exception(tr("Unexpected symbol '%1' at position %2.").arg(m_source[m_pos]).arg(m_pos));
    }

    Token Lexer::readNumberToken() {
        int startPos = m_pos;
        while(true) {
            switch(m_source[m_pos].unicode()) {
            case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': case L'8': case L'9':
                m_pos++;
                break;
            default:
                return Token(Number, m_source.midRef(startPos, m_pos - startPos), startPos);
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
                return Token(Color, m_source.midRef(startPos, m_pos - startPos), startPos);
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
                return Token(Variable, m_source.midRef(startPos, m_pos - startPos), startPos);
            }
        }
    }

    Token Lexer::readSymbolToken(TokenType type) {
        m_pos++;
        return Token(type, m_source.midRef(m_pos - 1, 1), m_pos - 1);
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
        throw Exception(tr("Unexpected token %1 ('%2') at position %3.").arg(serialized(token.type())).arg(token.text().toString()).arg(token.pos()));
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
                throw Exception(tr("Invalid color constant '%1'.").arg(token.text().toString()));
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
                throw Exception(tr("Invalid number constant '%1'.").arg(token.text().toString()));
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

    /* Color table from Qt.
     * CSS color names = SVG 1.0 color names + transparent. */
#undef rgb
#define rgb(r,g,b) (0xff000000 | (r << 16) |  (g << 8) | b)
    static const struct RGBData {
        const char *name;
        quint32 value;
    } rgbTable[] = {
        { "aliceblue", rgb(240, 248, 255) },
        { "antiquewhite", rgb(250, 235, 215) },
        { "aqua", rgb( 0, 255, 255) },
        { "aquamarine", rgb(127, 255, 212) },
        { "azure", rgb(240, 255, 255) },
        { "beige", rgb(245, 245, 220) },
        { "bisque", rgb(255, 228, 196) },
        { "black", rgb( 0, 0, 0) },
        { "blanchedalmond", rgb(255, 235, 205) },
        { "blue", rgb( 0, 0, 255) },
        { "blueviolet", rgb(138, 43, 226) },
        { "brown", rgb(165, 42, 42) },
        { "burlywood", rgb(222, 184, 135) },
        { "cadetblue", rgb( 95, 158, 160) },
        { "chartreuse", rgb(127, 255, 0) },
        { "chocolate", rgb(210, 105, 30) },
        { "coral", rgb(255, 127, 80) },
        { "cornflowerblue", rgb(100, 149, 237) },
        { "cornsilk", rgb(255, 248, 220) },
        { "crimson", rgb(220, 20, 60) },
        { "cyan", rgb( 0, 255, 255) },
        { "darkblue", rgb( 0, 0, 139) },
        { "darkcyan", rgb( 0, 139, 139) },
        { "darkgoldenrod", rgb(184, 134, 11) },
        { "darkgray", rgb(169, 169, 169) },
        { "darkgreen", rgb( 0, 100, 0) },
        { "darkgrey", rgb(169, 169, 169) },
        { "darkkhaki", rgb(189, 183, 107) },
        { "darkmagenta", rgb(139, 0, 139) },
        { "darkolivegreen", rgb( 85, 107, 47) },
        { "darkorange", rgb(255, 140, 0) },
        { "darkorchid", rgb(153, 50, 204) },
        { "darkred", rgb(139, 0, 0) },
        { "darksalmon", rgb(233, 150, 122) },
        { "darkseagreen", rgb(143, 188, 143) },
        { "darkslateblue", rgb( 72, 61, 139) },
        { "darkslategray", rgb( 47, 79, 79) },
        { "darkslategrey", rgb( 47, 79, 79) },
        { "darkturquoise", rgb( 0, 206, 209) },
        { "darkviolet", rgb(148, 0, 211) },
        { "deeppink", rgb(255, 20, 147) },
        { "deepskyblue", rgb( 0, 191, 255) },
        { "dimgray", rgb(105, 105, 105) },
        { "dimgrey", rgb(105, 105, 105) },
        { "dodgerblue", rgb( 30, 144, 255) },
        { "firebrick", rgb(178, 34, 34) },
        { "floralwhite", rgb(255, 250, 240) },
        { "forestgreen", rgb( 34, 139, 34) },
        { "fuchsia", rgb(255, 0, 255) },
        { "gainsboro", rgb(220, 220, 220) },
        { "ghostwhite", rgb(248, 248, 255) },
        { "gold", rgb(255, 215, 0) },
        { "goldenrod", rgb(218, 165, 32) },
        { "gray", rgb(128, 128, 128) },
        { "green", rgb( 0, 128, 0) },
        { "greenyellow", rgb(173, 255, 47) },
        { "grey", rgb(128, 128, 128) },
        { "honeydew", rgb(240, 255, 240) },
        { "hotpink", rgb(255, 105, 180) },
        { "indianred", rgb(205, 92, 92) },
        { "indigo", rgb( 75, 0, 130) },
        { "ivory", rgb(255, 255, 240) },
        { "khaki", rgb(240, 230, 140) },
        { "lavender", rgb(230, 230, 250) },
        { "lavenderblush", rgb(255, 240, 245) },
        { "lawngreen", rgb(124, 252, 0) },
        { "lemonchiffon", rgb(255, 250, 205) },
        { "lightblue", rgb(173, 216, 230) },
        { "lightcoral", rgb(240, 128, 128) },
        { "lightcyan", rgb(224, 255, 255) },
        { "lightgoldenrodyellow", rgb(250, 250, 210) },
        { "lightgray", rgb(211, 211, 211) },
        { "lightgreen", rgb(144, 238, 144) },
        { "lightgrey", rgb(211, 211, 211) },
        { "lightpink", rgb(255, 182, 193) },
        { "lightsalmon", rgb(255, 160, 122) },
        { "lightseagreen", rgb( 32, 178, 170) },
        { "lightskyblue", rgb(135, 206, 250) },
        { "lightslategray", rgb(119, 136, 153) },
        { "lightslategrey", rgb(119, 136, 153) },
        { "lightsteelblue", rgb(176, 196, 222) },
        { "lightyellow", rgb(255, 255, 224) },
        { "lime", rgb( 0, 255, 0) },
        { "limegreen", rgb( 50, 205, 50) },
        { "linen", rgb(250, 240, 230) },
        { "magenta", rgb(255, 0, 255) },
        { "maroon", rgb(128, 0, 0) },
        { "mediumaquamarine", rgb(102, 205, 170) },
        { "mediumblue", rgb( 0, 0, 205) },
        { "mediumorchid", rgb(186, 85, 211) },
        { "mediumpurple", rgb(147, 112, 219) },
        { "mediumseagreen", rgb( 60, 179, 113) },
        { "mediumslateblue", rgb(123, 104, 238) },
        { "mediumspringgreen", rgb( 0, 250, 154) },
        { "mediumturquoise", rgb( 72, 209, 204) },
        { "mediumvioletred", rgb(199, 21, 133) },
        { "midnightblue", rgb( 25, 25, 112) },
        { "mintcream", rgb(245, 255, 250) },
        { "mistyrose", rgb(255, 228, 225) },
        { "moccasin", rgb(255, 228, 181) },
        { "navajowhite", rgb(255, 222, 173) },
        { "navy", rgb( 0, 0, 128) },
        { "oldlace", rgb(253, 245, 230) },
        { "olive", rgb(128, 128, 0) },
        { "olivedrab", rgb(107, 142, 35) },
        { "orange", rgb(255, 165, 0) },
        { "orangered", rgb(255, 69, 0) },
        { "orchid", rgb(218, 112, 214) },
        { "palegoldenrod", rgb(238, 232, 170) },
        { "palegreen", rgb(152, 251, 152) },
        { "paleturquoise", rgb(175, 238, 238) },
        { "palevioletred", rgb(219, 112, 147) },
        { "papayawhip", rgb(255, 239, 213) },
        { "peachpuff", rgb(255, 218, 185) },
        { "peru", rgb(205, 133, 63) },
        { "pink", rgb(255, 192, 203) },
        { "plum", rgb(221, 160, 221) },
        { "powderblue", rgb(176, 224, 230) },
        { "purple", rgb(128, 0, 128) },
        { "red", rgb(255, 0, 0) },
        { "rosybrown", rgb(188, 143, 143) },
        { "royalblue", rgb( 65, 105, 225) },
        { "saddlebrown", rgb(139, 69, 19) },
        { "salmon", rgb(250, 128, 114) },
        { "sandybrown", rgb(244, 164, 96) },
        { "seagreen", rgb( 46, 139, 87) },
        { "seashell", rgb(255, 245, 238) },
        { "sienna", rgb(160, 82, 45) },
        { "silver", rgb(192, 192, 192) },
        { "skyblue", rgb(135, 206, 235) },
        { "slateblue", rgb(106, 90, 205) },
        { "slategray", rgb(112, 128, 144) },
        { "slategrey", rgb(112, 128, 144) },
        { "snow", rgb(255, 250, 250) },
        { "springgreen", rgb( 0, 255, 127) },
        { "steelblue", rgb( 70, 130, 180) },
        { "tan", rgb(210, 180, 140) },
        { "teal", rgb( 0, 128, 128) },
        { "thistle", rgb(216, 191, 216) },
        { "tomato", rgb(255, 99, 71) },
        { "transparent", 0 },
        { "turquoise", rgb( 64, 224, 208) },
        { "violet", rgb(238, 130, 238) },
        { "wheat", rgb(245, 222, 179) },
        { "white", rgb(255, 255, 255) },
        { "whitesmoke", rgb(245, 245, 245) },
        { "yellow", rgb(255, 255, 0) },
        { "yellowgreen", rgb(154, 205, 50) }
    };

    static const int rgbTableSize = sizeof(rgbTable) / sizeof(RGBData);
#undef rgb


// -------------------------------------------------------------------------- //
// Evaluator
// -------------------------------------------------------------------------- //
    Evaluator::Evaluator() {
        m_functionTypeId = qMetaTypeId<Function>();
        m_resolver = NULL;
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

    void Evaluator::registerFunctions(StandardFunctions functions) {
        if(functions & ColorFunctions) {
            registerFunction(lit("QColor"),             &eval_QColor);
            registerFunction(lit("QColor::lighter"),    &eval_QColor_lighter);
            registerFunction(lit("QColor::darker"),     &eval_QColor_darker);
            registerFunction(lit("QColor::setRed"),     &eval_QColor_setRed);
            registerFunction(lit("QColor::setGreen"),   &eval_QColor_setGreen);
            registerFunction(lit("QColor::setBlue"),    &eval_QColor_setBlue);
            registerFunction(lit("QColor::setAlpha"),   &eval_QColor_setAlpha);
        }

        if(functions & ColorNames)
            for(int i = 0; i < rgbTableSize; i++)
                registerConstant(QLatin1String(rgbTable[i].name), QVariant::fromValue(QColor(rgbTable[i].value)));
    }

    QVariant Evaluator::evaluate(const Program &program) const {
        QVector<QVariant> stack;

        for(const Instruction &instruction: program)
            exec(stack, instruction);

        if(stack.size() != 1)
            throw Exception(tr("Invalid stack size after program evaluation: %1.").arg(program.size()));

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
            assert(false);
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
            throw Exception(tr("Could not deduce result type for operation %1('%2', '%3').").arg(serialized(op)).arg(QLatin1String(l.typeName())).arg(QLatin1String(r.typeName())));

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
        default:    assert(false); return 0;
        }
    }

    double Evaluator::binop(double l, double r, InstructionType op) const {
        switch (op) {
        case Add:   return l + r;
        case Sub:   return l - r;
        case Mul:   return l * r;
        case Div:   return l / r;
        case And:
        case Or:    throw IllegalArgumentException(tr("Invalid parameter type for operation %1('%2', '%2')").arg(serialized(op)).arg(lit("double")));
        default:    assert(false); return 0;
        }
    }

    void Evaluator::unop(QVector<QVariant> &stack, InstructionType op) const {
        QVariant a = pop_back(stack);

        int type = upperType(a.userType());
        if(type == QMetaType::UnknownType)
            throw Exception(tr("Could not deduce arithmetic supertype for type '%1'.").arg(QLatin1String(a.typeName())));

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
        default:    assert(false); return 0;
        }
    }

    double Evaluator::unop(double v, InstructionType op) const {
        switch (op) {
        case Neg:   return -v;
        case Udd:   return v;
        case Not:   throw IllegalArgumentException(tr("Invalid parameter type for operation %1('%2')").arg(serialized(op)).arg(lit("double")));
        default:    assert(false); return 0;
        }
    }

    void Evaluator::call(QVector<QVariant> &stack, const Instruction &instruction) const {
        QVariant argcVariant = instruction.data();
        if(argcVariant.userType() != QMetaType::Int)
            throw Exception(tr("Argument number for %1 instruction has invalid type '%2'.").arg(serialized(instruction.type())).arg(QLatin1String(argcVariant.typeName())));
        int argc = argcVariant.toInt();

        if(argc < 0)
            throw Exception(tr("Argument number for %1 instruction is invalid.").arg(serialized(instruction.type())));
        if(instruction.type() == MCall)
            argc++; /* 'this' is treated as another argument. */ 

        if(stack.size() < argc + 1)
            throw Exception(tr("Stack underflow during execution of %1 instruction.").arg(serialized(instruction.type())));

        QVariant nameVariant = stack[stack.size() - 1];
        if(nameVariant.userType() != QMetaType::QString)
            throw Exception(tr("Function name for %1 instruction has invalid type '%2'.").arg(serialized(instruction.type())).arg(QLatin1String(nameVariant.typeName())));
        QString name = nameVariant.toString();

        if(instruction.type() == MCall)
            name = QLatin1String(stack[stack.size() - argc - 1].typeName()) + lit("::") + name;

        QVariant variable = m_constants.value(name);
        if(variable.userType() == QMetaType::UnknownType) {
            if(m_resolver)
                variable = m_resolver->resolveConstant(name);
            if(variable.userType() == QMetaType::UnknownType)
                throw Exception(tr("Function or variable '%1' is not defined.").arg(name));
        }

        QVariant result;
        if(variable.userType() != m_functionTypeId) {
            if(argc > 0)
                throw Exception(tr("Variable '%1' is not a function and cannot be called.").arg(name));
            result = variable;
        } else {
            result = variable.value<Function>()(ParameterPack(stack, argc, name));
        }

        stack.resize(stack.size() - argc - 1);
        stack.push_back(result);
    }

    QVariant Evaluator::pop_back(QVector<QVariant> &stack) {
        if(stack.isEmpty())
            throw Exception(tr("Stack underflow during program evaluation."));

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

