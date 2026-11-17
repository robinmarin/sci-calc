#include "engine.h"

#include <QCoreApplication>
#include <QHash>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace {

constexpr long double Pi = 3.141592653589793238462643383279502884L;
constexpr long double E = 2.718281828459045235360287471352662498L;
constexpr long double Phi = 1.618033988749894848204586834365638118L;
const long double NaN = std::nanl("");

QString tr(const char *text) { return QCoreApplication::translate("Engine", text); }

struct Token {
    enum Type { Number, Ident, Op, End } type = End;
    QString text;
    long double value = 0;
    int pos = 0;
    QChar op() const { return type == Op ? text.at(0) : QChar(); }
};

struct Failure {
    QString message;
    int pos;
};

// Unicode spellings people paste or that the keypad inserts, mapped onto the
// operators the parser knows. '>' stands for the store arrow internally.
QChar normalizeOperator(QChar c) {
    switch (c.unicode()) {
    case 0x00D7: case 0x00B7: case 0x22C5: case 0x2217: return '*'; // × · ⋅ ∗
    case 0x00F7: case 0x2215: return '/';                            // ÷ ∕
    case 0x2212: case 0x2013: return '-';                            // − –
    case 0x2192: return '>';                                         // →
    default: return c;
    }
}

std::vector<Token> tokenize(const QString &s) {
    std::vector<Token> tokens;
    int i = 0;
    const int n = s.size();
    auto isDigit = [](QChar c) { return c >= '0' && c <= '9'; };

    while (i < n) {
        const QChar c = s.at(i);
        if (c.isSpace()) { ++i; continue; }

        const int start = i;

        // Hex, binary, and octal literals, handy for one-off conversions.
        if (c == '0' && i + 1 < n && QString("xXbBoO").contains(s.at(i + 1))) {
            const QChar kind = s.at(i + 1).toLower();
            const int base = kind == 'x' ? 16 : kind == 'b' ? 2 : 8;
            int j = i + 2;
            auto isBaseDigit = [&](QChar d) {
                return isDigit(d) || d == '_' || (base == 16 && QString("abcdefABCDEF").contains(d));
            };
            while (j < n && isBaseDigit(s.at(j))) ++j;
            QString digits = s.mid(i + 2, j - i - 2).remove('_');
            bool ok = false;
            const qulonglong v = digits.toULongLong(&ok, base);
            if (!ok || digits.isEmpty())
                throw Failure{tr("Bad number"), start};
            tokens.push_back({Token::Number, s.mid(i, j - i), static_cast<long double>(v), start});
            i = j;
            continue;
        }

        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(s.at(i + 1)))) {
            QString text;
            bool seenDot = false;
            while (i < n && (isDigit(s.at(i)) || (s.at(i) == '.' && !seenDot) || s.at(i) == '_')) {
                if (s.at(i) == '.') seenDot = true;
                if (s.at(i) != '_') text += s.at(i);
                ++i;
            }
            // An exponent only when digits follow, so "2e" still means 2·e.
            if (i < n && (s.at(i) == 'e' || s.at(i) == 'E' || s.at(i).unicode() == 0x1D07)) {
                int j = i + 1;
                if (j < n && (s.at(j) == '+' || s.at(j) == '-' || s.at(j).unicode() == 0x2212)) ++j;
                if (j < n && isDigit(s.at(j))) {
                    text += 'e';
                    for (int k = i + 1; k < j; ++k) text += s.at(k) == '+' ? '+' : '-';
                    i = j;
                    while (i < n && isDigit(s.at(i))) text += s.at(i++);
                }
            }
            const QByteArray ascii = text.toLatin1();
            tokens.push_back({Token::Number, text, std::strtold(ascii.constData(), nullptr), start});
            continue;
        }

        if (c.isLetter() || c == '_') {
            int j = i;
            // Names are letters, ASCII digits, and underscores; π and
            // superscripts end a name, so "xπ" and "x²" parse as expected.
            while (j < n && ((s.at(j).isLetter() && s.at(j).unicode() != 0x03C0) || isDigit(s.at(j)) || s.at(j) == '_'))
                ++j;
            if (j == i) j = i + 1;
            QString name = s.mid(i, j - i);
            if (name == QString(QChar(0x03C0))) name = "pi";
            tokens.push_back({Token::Ident, name, 0, start});
            i = j;
            continue;
        }

        // Single-character spellings of common functions and powers.
        if (c.unicode() == 0x221A) { tokens.push_back({Token::Ident, "sqrt", 0, start}); ++i; continue; }
        if (c.unicode() == 0x221B) { tokens.push_back({Token::Ident, "cbrt", 0, start}); ++i; continue; }
        if (c.unicode() == 0x00B2) { tokens.push_back({Token::Op, "2", 0, start}); ++i; continue; } // ²
        if (c.unicode() == 0x00B3) { tokens.push_back({Token::Op, "3", 0, start}); ++i; continue; } // ³

        if (c == '*' && i + 1 < n && s.at(i + 1) == '*') { tokens.push_back({Token::Op, "^", 0, start}); i += 2; continue; }
        if (c == '-' && i + 1 < n && s.at(i + 1) == '>') { tokens.push_back({Token::Op, ">", 0, start}); i += 2; continue; }

        const QChar op = normalizeOperator(c);
        if (QString("+-*/^%!(),=>").contains(op)) {
            tokens.push_back({Token::Op, QString(op), 0, start});
            ++i;
            continue;
        }

        throw Failure{tr("Unexpected \"%1\"").arg(c), start};
    }
    tokens.push_back({Token::End, {}, 0, static_cast<int>(n)});
    return tokens;
}

struct Function {
    int minArgs;
    int maxArgs; // -1 for any number.
    std::function<long double(const std::vector<long double> &, Engine::Angle)> apply;
};

bool isInteger(long double v) { return std::isfinite(v) && v == std::floor(v); }

long double fromRadians(long double x, Engine::Angle a) { return a == Engine::Angle::Degrees ? x * 180 / Pi : x; }

// A result this close to zero, from an argument that is not itself tiny, is
// sin(π) style rounding noise and is shown as an exact zero.
long double snap(long double result, long double argument) {
    return std::fabs(argument) > 1e-3L && std::fabs(result) < 1e-15L ? 0 : result;
}

// Degrees are reduced before converting, so sin(180) and cos(90) are exact.
long double sine(long double x, Engine::Angle a) {
    if (a == Engine::Angle::Degrees) {
        const long double r = std::fmod(x, 360.0L);
        if (std::fmod(r, 90.0L) == 0) {
            const int quarter = static_cast<int>(std::lround(r / 90)) % 4;
            static const int table[] = {0, 1, 0, -1};
            return table[(quarter + 4) % 4];
        }
        if (std::fmod(r, 30.0L) == 0 && std::fmod(r, 60.0L) != 0)
            return std::sin(r * Pi / 180) > 0 ? 0.5L : -0.5L;
        return std::sin(r * Pi / 180);
    }
    return snap(std::sin(x), x);
}

long double cosine(long double x, Engine::Angle a) {
    if (a == Engine::Angle::Degrees)
        return sine(x + 90, a);
    return snap(std::cos(x), x);
}

long double tangent(long double x, Engine::Angle a) {
    const long double c = cosine(x, a);
    if (c == 0) return NaN;
    if (a == Engine::Angle::Degrees && std::fmod(x, 45.0L) == 0 && std::fmod(x, 90.0L) != 0)
        return sine(x, a) / c > 0 ? 1 : -1;
    return snap(sine(x, a) / c, x);
}

long double factorial(long double x) {
    if (x < 0 && isInteger(x)) return NaN;
    if (isInteger(x) && x <= 170) {
        long double r = 1;
        for (int i = 2; i <= static_cast<int>(x); ++i) r *= i;
        return r;
    }
    return std::tgamma(x + 1);
}

long double choose(long double n, long double r, bool ordered) {
    if (!isInteger(n) || !isInteger(r) || n < 0 || r < 0) return NaN;
    if (r > n) return 0;
    long double result = 1;
    if (!ordered && r > n - r) r = n - r;
    for (long double i = 0; i < r; ++i) {
        result *= (n - i);
        if (!ordered) result /= (i + 1);
    }
    return std::round(result);
}

long double gcd(long double a, long double b) {
    a = std::fabs(a); b = std::fabs(b);
    while (b != 0) { const long double t = std::fmod(a, b); a = b; b = t; }
    return a;
}

// Python's modulo: the result takes the sign of the divisor.
long double modulo(long double a, long double b) {
    const long double r = std::fmod(a, b);
    return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
}

const QHash<QString, Function> &functions() {
    using V = std::vector<long double>;
    using A = Engine::Angle;
    static const QHash<QString, Function> table = [] {
        QHash<QString, Function> t;
        auto one = [&](const QString &name, std::function<long double(long double, A)> f) {
            t.insert(name, {1, 1, [f](const V &v, A a) { return f(v[0], a); }});
        };
        auto plain = [&](const QString &name, long double (*f)(long double)) {
            t.insert(name, {1, 1, [f](const V &v, A) { return f(v[0]); }});
        };

        one("sin", sine);
        one("cos", cosine);
        one("tan", tangent);
        one("sec", [](long double x, A a) { const long double c = cosine(x, a); return c == 0 ? NaN : 1 / c; });
        one("csc", [](long double x, A a) { const long double s = sine(x, a); return s == 0 ? NaN : 1 / s; });
        one("cot", [](long double x, A a) { const long double s = sine(x, a); return s == 0 ? NaN : cosine(x, a) / s; });
        one("asin", [](long double x, A a) { return fromRadians(std::asin(x), a); });
        one("acos", [](long double x, A a) { return fromRadians(std::acos(x), a); });
        one("atan", [](long double x, A a) { return fromRadians(std::atan(x), a); });
        t.insert("arcsin", t.value("asin"));
        t.insert("arccos", t.value("acos"));
        t.insert("arctan", t.value("atan"));
        t.insert("atan2", {2, 2, [](const V &v, A a) { return fromRadians(std::atan2(v[0], v[1]), a); }});

        plain("sinh", std::sinh);
        plain("cosh", std::cosh);
        plain("tanh", std::tanh);
        plain("asinh", std::asinh);
        plain("acosh", std::acosh);
        plain("atanh", std::atanh);

        one("ln", [](long double x, A) { return x <= 0 ? NaN : std::log(x); });
        t.insert("log", {1, 2, [](const V &v, A) {
            if (v[0] <= 0) return NaN;
            if (v.size() == 1) return std::log10(v[0]);
            if (v[1] <= 0 || v[1] == 1) return NaN;
            return std::log(v[0]) / std::log(v[1]);
        }});
        one("log2", [](long double x, A) { return x <= 0 ? NaN : std::log2(x); });
        plain("exp", std::exp);
        plain("sqrt", std::sqrt);
        plain("cbrt", std::cbrt);
        t.insert("root", {2, 2, [](const V &v, A) {
            const long double x = v[0], n = v[1];
            if (n == 0) return NaN;
            if (x < 0) {
                if (isInteger(n) && std::fmod(std::fabs(n), 2.0L) == 1) return -std::pow(-x, 1 / n);
                return NaN;
            }
            return std::pow(x, 1 / n);
        }});

        plain("abs", std::fabs);
        plain("floor", std::floor);
        plain("ceil", std::ceil);
        plain("int", std::trunc);
        plain("ipart", std::trunc);
        one("fpart", [](long double x, A) { return x - std::trunc(x); });
        one("sign", [](long double x, A) { return static_cast<long double>((x > 0) - (x < 0)); });
        t.insert("sgn", t.value("sign"));
        t.insert("round", {1, 2, [](const V &v, A) {
            if (v.size() == 1) return std::round(v[0]);
            if (!isInteger(v[1])) return NaN;
            const long double scale = std::pow(10.0L, v[1]);
            return std::round(v[0] * scale) / scale;
        }});

        one("fact", [](long double x, A) { return factorial(x); });
        t.insert("nCr", {2, 2, [](const V &v, A) { return choose(v[0], v[1], false); }});
        t.insert("nPr", {2, 2, [](const V &v, A) { return choose(v[0], v[1], true); }});
        t.insert("gcd", {2, -1, [](const V &v, A) {
            long double g = 0;
            for (long double x : v) { if (!isInteger(x)) return NaN; g = gcd(g, x); }
            return g;
        }});
        t.insert("lcm", {2, -1, [](const V &v, A) {
            long double l = 1;
            for (long double x : v) {
                if (!isInteger(x)) return NaN;
                if (x == 0) return 0.0L;
                l = std::fabs(l / gcd(l, x) * x);
            }
            return l;
        }});
        t.insert("mod", {2, 2, [](const V &v, A) { return v[1] == 0 ? NaN : modulo(v[0], v[1]); }});
        t.insert("hypot", {2, 2, [](const V &v, A) { return std::hypot(v[0], v[1]); }});
        t.insert("min", {1, -1, [](const V &v, A) { long double m = v[0]; for (auto x : v) m = std::min(m, x); return m; }});
        t.insert("max", {1, -1, [](const V &v, A) { long double m = v[0]; for (auto x : v) m = std::max(m, x); return m; }});
        t.insert("sum", {1, -1, [](const V &v, A) { long double s = 0; for (auto x : v) s += x; return s; }});
        t.insert("mean", {1, -1, [](const V &v, A) { long double s = 0; for (auto x : v) s += x; return s / v.size(); }});
        t.insert("avg", t.value("mean"));

        // Conversions that ignore the angle mode: rad(180) is π, deg(π) is 180.
        one("rad", [](long double x, A) { return x * Pi / 180; });
        one("deg", [](long double x, A) { return x * 180 / Pi; });
        return t;
    }();
    return table;
}

const QHash<QString, long double> &constants() {
    static const QHash<QString, long double> table{
        {"pi", Pi}, {"tau", 2 * Pi}, {"e", E}, {"phi", Phi},
    };
    return table;
}

class Parser {
public:
    Parser(std::vector<Token> tokens, Engine &engine) : m_tokens(std::move(tokens)), m_engine(engine) {}

    long double expression() {
        long double v = term();
        while (peek().op() == '+' || peek().op() == '-') {
            const bool add = next().op() == '+';
            const long double rhs = term();
            v = add ? v + rhs : v - rhs;
        }
        return v;
    }

    const Token &peek(int ahead = 0) const {
        const size_t i = std::min(m_index + ahead, m_tokens.size() - 1);
        return m_tokens[i];
    }
    const Token &next() { const Token &t = peek(); if (m_index < m_tokens.size() - 1) ++m_index; return t; }

private:
    // A primary that follows another with no operator multiplies, TI style:
    // 2π, 3(4+1), 2sin(30), (1+2)(3+4).
    bool startsImplicitFactor() const {
        const Token &t = peek();
        return t.type == Token::Ident || t.op() == '(';
    }

    long double term() {
        long double v = unary();
        for (;;) {
            const QChar op = peek().op();
            if (op == '*' || op == '/' || op == '%') {
                const Token &t = next();
                const long double rhs = unary();
                if (op == '*') v *= rhs;
                else if (rhs == 0) throw Failure{tr("Division by zero"), t.pos};
                else v = op == '/' ? v / rhs : modulo(v, rhs);
            } else if (startsImplicitFactor()) {
                v *= unary();
            } else {
                return v;
            }
        }
    }

    // Negation binds looser than powers, so -2^2 is -4 as on paper.
    long double unary() {
        if (peek().op() == '-') { next(); return -unary(); }
        if (peek().op() == '+') { next(); return unary(); }
        return power();
    }

    long double power() {
        long double base = postfix();
        if (peek().op() == '^') {
            const Token &t = next();
            const long double exponent = unary();
            return checked(std::pow(base, exponent), t.pos, tr("Domain error in ^"));
        }
        return base;
    }

    long double postfix() {
        long double v = primary();
        for (;;) {
            const Token &t = peek();
            if (t.op() == '!') { next(); v = checked(factorial(v), t.pos, tr("Domain error in !")); }
            else if (t.op() == '2') { next(); v = v * v; }
            else if (t.op() == '3') { next(); v = v * v * v; }
            else return v;
        }
    }

    long double primary() {
        const Token t = next();
        switch (t.type) {
        case Token::Number:
            return t.value;
        case Token::Op:
            if (t.op() == '(') {
                const long double v = expression();
                closeParen();
                return v;
            }
            throw Failure{tr("Unexpected \"%1\"").arg(t.text), t.pos};
        case Token::End:
            throw Failure{tr("Incomplete expression"), t.pos};
        case Token::Ident:
            return identifier(t);
        }
        return 0;
    }

    // A missing closing parenthesis at the end of the line is forgiven, the
    // way TI calculators accept "sin(30".
    void closeParen() {
        if (peek().op() == ')') { next(); return; }
        if (peek().type == Token::End) return;
        throw Failure{tr("Expected \")\""), peek().pos};
    }

    long double identifier(const Token &t) {
        const auto &fns = functions();
        const auto fn = fns.constFind(t.text);
        if (fn != fns.constEnd()) {
            std::vector<long double> args;
            if (peek().op() == '(') {
                next();
                if (peek().op() != ')') {
                    args.push_back(expression());
                    while (peek().op() == ',') { next(); args.push_back(expression()); }
                }
                closeParen();
            } else {
                // "sin 30" and "√9" take the next operand without parentheses.
                args.push_back(unary());
            }
            const int count = static_cast<int>(args.size());
            if (count < fn->minArgs || (fn->maxArgs >= 0 && count > fn->maxArgs)) {
                const QString expected = fn->minArgs == fn->maxArgs ? QString::number(fn->minArgs)
                                         : fn->maxArgs < 0 ? QString("%1+").arg(fn->minArgs)
                                                           : QString("%1–%2").arg(fn->minArgs).arg(fn->maxArgs);
                throw Failure{tr("%1 takes %2 arguments").arg(t.text, expected), t.pos};
            }
            return checked(fn->apply(args, m_engine.angle()), t.pos, tr("Domain error in %1").arg(t.text));
        }
        if (t.text == "ans") return m_engine.ans();
        const auto c = constants().constFind(t.text);
        if (c != constants().constEnd()) return *c;
        const auto var = m_engine.variables().constFind(t.text);
        if (var != m_engine.variables().constEnd()) return *var;
        throw Failure{tr("Unknown name \"%1\"").arg(t.text), t.pos};
    }

    long double checked(long double v, int pos, const QString &domainMessage) {
        if (std::isnan(v)) throw Failure{domainMessage, pos};
        if (std::isinf(v)) throw Failure{tr("Overflow"), pos};
        return v;
    }

    std::vector<Token> m_tokens;
    size_t m_index = 0;
    Engine &m_engine;
};

} // namespace

bool Engine::isReserved(const QString &name) {
    return name == "ans" || functions().contains(name) || constants().contains(name);
}

QStringList Engine::functionNames() {
    QStringList names = functions().keys();
    names.sort();
    return names;
}

Engine::Result Engine::evaluate(const QString &line) {
    Result result;
    try {
        std::vector<Token> tokens = tokenize(line);
        if (tokens.size() == 1)
            throw Failure{tr("Nothing to calculate"), 0};

        // "x = 2+3" stores into x.
        QString target;
        if (tokens.size() > 2 && tokens[0].type == Token::Ident && tokens[1].op() == '=') {
            target = tokens[0].text;
            if (isReserved(target))
                throw Failure{tr("\"%1\" is a built-in name").arg(target), tokens[0].pos};
            tokens.erase(tokens.begin(), tokens.begin() + 2);
        }

        Parser parser(tokens, *this);
        long double value = parser.expression();

        // "2+3 → x" stores too, as on a TI.
        if (parser.peek().op() == '>') {
            parser.next();
            const Token name = parser.next();
            if (name.type != Token::Ident)
                throw Failure{tr("Expected a variable name"), name.pos};
            if (isReserved(name.text))
                throw Failure{tr("\"%1\" is a built-in name").arg(name.text), name.pos};
            target = name.text;
        }
        if (parser.peek().type != Token::End) {
            const Token &t = parser.peek();
            throw Failure{t.op() == ')' ? tr("Unmatched \")\"") : tr("Unexpected \"%1\"").arg(t.text), t.pos};
        }
        if (!std::isfinite(value))
            throw Failure{tr("Overflow"), 0};
        if (value == 0) value = 0; // No negative zero.

        if (!target.isEmpty()) {
            m_variables[target] = value;
            result.assigned = target;
        }
        m_ans = value;
        result.ok = true;
        result.value = value;
    } catch (const Failure &failure) {
        result.error = failure.message;
        result.errorPosition = failure.pos;
    }
    return result;
}

QString Engine::format(long double value) {
    if (value == 0 || std::fabs(value) < 1e-300L)
        return "0";
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.12Lg", value);
    QString text = QString::fromLatin1(buffer);
    // "1.5e+20" reads back fine, but "1.5e20" is what a person types.
    const int e = text.indexOf('e');
    if (e >= 0) {
        QString mantissa = text.left(e);
        QString exponent = text.mid(e + 1);
        const bool negative = exponent.startsWith('-');
        exponent.remove(0, 1);
        while (exponent.size() > 1 && exponent.startsWith('0')) exponent.remove(0, 1);
        text = mantissa + "e" + (negative ? "-" : "") + exponent;
    }
    return text;
}

QString Engine::groupThousands(const QString &formatted) {
    if (formatted.contains('e'))
        return formatted;
    const bool negative = formatted.startsWith('-');
    const QString body = negative ? formatted.mid(1) : formatted;
    const int dot = body.indexOf('.');
    QString whole = dot < 0 ? body : body.left(dot);
    for (int at = whole.size() - 3; at > 0; at -= 3)
        whole.insert(at, ',');
    return (negative ? "-" : "") + whole + (dot < 0 ? QString() : body.mid(dot));
}

// Past this, a fraction is no easier to read than the decimal.
constexpr long double MaxNumerator = 1e6L;
constexpr long double MaxDenominator = 10000;

bool Engine::isReadableFraction(const QString &fraction) {
    const QStringList parts = QString(fraction).remove('-').split('/');
    bool okTop = false, okBottom = false;
    if (parts.size() != 2) return false;
    const double top = parts[0].toDouble(&okTop), bottom = parts[1].toDouble(&okBottom);
    return okTop && okBottom && top < MaxNumerator && bottom > 1 && bottom <= MaxDenominator;
}

QString Engine::fraction(long double value) {
    if (!std::isfinite(value) || isInteger(value) || std::fabs(value) >= MaxNumerator)
        return {};
    const long double x = std::fabs(value);
    // Continued-fraction convergents until the denominator grows too large.
    long double h0 = 0, h1 = 1, k0 = 1, k1 = 0, rest = x;
    for (int i = 0; i < 40; ++i) {
        const long double a = std::floor(rest);
        const long double h2 = a * h1 + h0, k2 = a * k1 + k0;
        if (k2 > MaxDenominator) break;
        h0 = h1; h1 = h2; k0 = k1; k1 = k2;
        // A real fraction matches to long double precision (about 1e-19);
        // a looser test lets large irrational answers pass for a fraction
        // with a nine-digit numerator, as e^10*5 did.
        if (std::fabs(x - h1 / k1) <= 1e-15L * std::max(1.0L, x)) {
            if (k1 == 1 || h1 >= MaxNumerator) return {};
            return QString("%1%2/%3").arg(value < 0 ? "-" : "").arg(static_cast<qlonglong>(h1)).arg(static_cast<qlonglong>(k1));
        }
        const long double frac = rest - a;
        if (frac < 1e-18L) break;
        rest = 1 / frac;
    }
    return {};
}
