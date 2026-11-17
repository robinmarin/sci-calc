#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

// The calculator itself: parses and evaluates one line at a time, keeping
// variables, the last answer, and the angle mode between lines. It knows
// nothing about Qt Quick, so the CLI, the GUI, and the tests share it.
class Engine {
public:
    enum class Angle { Degrees, Radians };

    struct Result {
        bool ok = false;
        long double value = 0;
        QString error;       // Human-readable reason when !ok.
        int errorPosition = -1; // Character index the error points at, or -1.
        QString assigned;    // Variable name when the line stored a value.
    };

    Result evaluate(const QString &line);

    Angle angle() const { return m_angle; }
    void setAngle(Angle angle) { m_angle = angle; }

    long double ans() const { return m_ans; }
    void setAns(long double value) { m_ans = value; }

    const QMap<QString, long double> &variables() const { return m_variables; }
    void setVariable(const QString &name, long double value) { m_variables[name] = value; }
    void clearVariables() { m_variables.clear(); }

    // True when the name belongs to a function or constant and cannot be
    // used as a variable.
    static bool isReserved(const QString &name);
    static QStringList functionNames();

    // Up to 12 significant digits, scientific notation for very large and
    // very small magnitudes, in a form the parser reads back.
    static QString format(long double value);
    // Adds thousands separators to a formatted number for display:
    // "1234567.891" becomes "1,234,567.891". Scientific notation is left alone.
    // Display only: the parser reads a comma as an argument separator.
    static QString groupThousands(const QString &formatted);
    // "3/8" when the value is a fraction with a small denominator, else "".
    static QString fraction(long double value);
    // Whether a stored "p/q" is one fraction() would still show; older
    // versions saved some that it no longer does.
    static bool isReadableFraction(const QString &fraction);

private:
    Angle m_angle = Angle::Radians;
    long double m_ans = 0;
    QMap<QString, long double> m_variables;
};
