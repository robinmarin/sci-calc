#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "engine.h"
#include "theme.h"
#include "translator.h"

class ScicalcTests : public QObject {
    Q_OBJECT

private:
    static QString calc(const QString &line, Engine::Angle angle = Engine::Angle::Radians) {
        Engine engine;
        engine.setAngle(angle);
        const auto r = engine.evaluate(line);
        return r.ok ? Engine::format(r.value) : "ERR: " + r.error;
    }
    static QString deg(const QString &line) { return calc(line, Engine::Angle::Degrees); }

    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

private slots:
    void arithmeticAndPrecedence() {
        QCOMPARE(calc("1+2*3"), QString("7"));
        QCOMPARE(calc("(1+2)*3"), QString("9"));
        QCOMPARE(calc("2^3^2"), QString("512"));
        QCOMPARE(calc("-2^2"), QString("-4"));
        QCOMPARE(calc("2^-1"), QString("0.5"));
        QCOMPARE(calc("2**10"), QString("1024"));
        QCOMPARE(calc("0.1+0.2"), QString("0.3"));
        QCOMPARE(calc("7 % 3"), QString("1"));
        QCOMPARE(calc("-7 % 3"), QString("2"));
        QCOMPARE(calc("10/4"), QString("2.5"));
        QCOMPARE(calc("3 × 4 ÷ 2 − 1"), QString("5"));
    }

    void impliedMultiplication() {
        QCOMPARE(calc("2pi"), calc("2*pi"));
        QCOMPARE(calc("2π"), calc("2*pi"));
        QCOMPARE(calc("3(4+1)"), QString("15"));
        QCOMPARE(calc("(1+2)(3+4)"), QString("21"));
        QCOMPARE(deg("2sin(30)"), QString("1"));
    }

    void postfixAndUnicode() {
        QCOMPARE(calc("5!"), QString("120"));
        QCOMPARE(calc("3²"), QString("9"));
        QCOMPARE(calc("2³"), QString("8"));
        QCOMPARE(calc("√16"), QString("4"));
        QCOMPARE(calc("√(9)+1"), QString("4"));
        QCOMPARE(calc("0.5!"), QString("0.886226925453"));
    }

    void numbersAndNotation() {
        QCOMPARE(calc("1.5e3"), QString("1500"));
        QCOMPARE(calc("1E-3"), QString("0.001"));
        QCOMPARE(calc("2e"), calc("2*e"));
        QCOMPARE(calc("0xff"), QString("255"));
        QCOMPARE(calc("0b101"), QString("5"));
        QCOMPARE(calc("0o17"), QString("15"));
        QCOMPARE(calc(".5*4"), QString("2"));
        QCOMPARE(calc("1_000_000"), QString("1000000"));
    }

    void trigonometry() {
        QCOMPARE(deg("sin(30)"), QString("0.5"));
        QCOMPARE(deg("cos(90)"), QString("0"));
        QCOMPARE(deg("cos(60)"), QString("0.5"));
        QCOMPARE(deg("tan(45)"), QString("1"));
        QCOMPARE(deg("sin(-30)"), QString("-0.5"));
        QCOMPARE(deg("asin(1)"), QString("90"));
        QVERIFY(deg("tan(90)").startsWith("ERR"));
        QCOMPARE(calc("sin(pi)"), QString("0"));
        QCOMPARE(calc("cos(pi)"), QString("-1"));
        QCOMPARE(calc("sin(1e-20)"), QString("1e-20"));
        QCOMPARE(calc("sin 0"), QString("0"));
    }

    void functions() {
        QCOMPARE(calc("ln(e)"), QString("1"));
        QCOMPARE(calc("log(1000)"), QString("3"));
        QCOMPARE(calc("log(8,2)"), QString("3"));
        QCOMPARE(calc("nCr(5,2)"), QString("10"));
        QCOMPARE(calc("nPr(5,2)"), QString("20"));
        QCOMPARE(calc("gcd(12,18)"), QString("6"));
        QCOMPARE(calc("lcm(4,6)"), QString("12"));
        QCOMPARE(calc("root(27,3)"), QString("3"));
        QCOMPARE(calc("root(-8,3)"), QString("-2"));
        QCOMPARE(calc("max(1,5,3)"), QString("5"));
        QCOMPARE(calc("mean(1,2,3,4)"), QString("2.5"));
        QCOMPARE(calc("round(2.345,2)"), QString("2.35"));
        QCOMPARE(calc("abs(-3)"), QString("3"));
    }

    void errors() {
        QVERIFY(calc("1/0").startsWith("ERR"));
        QVERIFY(calc("sqrt(-1)").startsWith("ERR"));
        QVERIFY(calc("ln(0)").startsWith("ERR"));
        QVERIFY(calc("2+").startsWith("ERR"));
        QVERIFY(calc("foo").startsWith("ERR"));
        QVERIFY(calc("1)").startsWith("ERR"));
        QVERIFY(calc("10^5000").startsWith("ERR"));
        QVERIFY(calc("nCr(5)").startsWith("ERR"));
        QCOMPARE(calc("sin(30"), calc("sin(30)"));

        Engine engine;
        const auto r = engine.evaluate("1 + foo");
        QVERIFY(!r.ok);
        QCOMPARE(r.errorPosition, 4);
    }

    void variablesAndAns() {
        Engine engine;
        QVERIFY(engine.evaluate("x = 4").ok);
        QCOMPARE(Engine::format(engine.evaluate("3x").value), QString("12"));
        QCOMPARE(Engine::format(engine.evaluate("ans+1").value), QString("13"));
        const auto stored = engine.evaluate("2+3 → y");
        QVERIFY(stored.ok);
        QCOMPARE(stored.assigned, QString("y"));
        QCOMPARE(Engine::format(engine.evaluate("x*y").value), QString("20"));
        QVERIFY(engine.evaluate("10 -> z").ok);
        QCOMPARE(Engine::format(engine.evaluate("z").value), QString("10"));
        QVERIFY(!engine.evaluate("pi = 3").ok);
        QVERIFY(!engine.evaluate("4 -> sin").ok);
    }

    void formatting() {
        QCOMPARE(Engine::format(1.0L / 3), QString("0.333333333333"));
        QCOMPARE(Engine::format(1e20L), QString("1e20"));
        QCOMPARE(Engine::format(-1.5e-7L), QString("-1.5e-7"));
        QCOMPARE(Engine::format(-0.0L), QString("0"));
        QCOMPARE(Engine::groupThousands("1234567.891"), QString("1,234,567.891"));
        QCOMPARE(Engine::groupThousands("-1000"), QString("-1,000"));
        QCOMPARE(Engine::groupThousands("999"), QString("999"));
        QCOMPARE(Engine::groupThousands("100000"), QString("100,000"));
        QCOMPARE(Engine::groupThousands("0.000123"), QString("0.000123"));
        QCOMPARE(Engine::groupThousands("1.5e20"), QString("1.5e20"));
        QCOMPARE(Engine::fraction(0.375L), QString("3/8"));
        QCOMPARE(Engine::fraction(-1.0L / 3), QString("-1/3"));
        QCOMPARE(Engine::fraction(2.0L), QString());
        QCOMPARE(Engine::fraction(3.141592653589793L), QString());
    }

    void backendChainsAndPersists() {
        QTemporaryDir dir;
        const QString state = dir.filePath("state/state.json");
        {
            Backend backend(state);
            QVERIFY(backend.evaluate("6"));
            QVERIFY(backend.evaluate("*7"));
            QCOMPARE(backend.lastResult(), QString("42"));
            QVERIFY(backend.evaluate(""));   // Repeats "ans*7".
            QCOMPARE(backend.lastResult(), QString("294"));
            QVERIFY(backend.evaluate("r = 2"));
            backend.setDegrees(true);
            QVERIFY(!backend.evaluate("1/0"));
            QVERIFY(!backend.error().isEmpty());
            QCOMPARE(backend.history().size(), 4);
        }
        Backend restored(state);
        QCOMPARE(restored.history().size(), 4);
        QVERIFY(restored.degrees());
        QVERIFY(restored.evaluate("ans + r"));
        QCOMPARE(restored.lastResult(), QString("4"));
        QVERIFY(restored.evaluate("sin(30)"));
        QCOMPARE(restored.lastResult(), QString("0.5"));
    }

    void ansPrefixRules() {
        QCOMPARE(Backend::withAns("+1"), QString("ans+1"));
        QCOMPARE(Backend::withAns("^2"), QString("ans^2"));
        QCOMPARE(Backend::withAns("-1"), QString("-1"));
        QCOMPARE(Backend::withAns("->x"), QString("ans->x"));
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }
};

QTEST_MAIN(ScicalcTests)
#include "scicalc_tests.moc"
