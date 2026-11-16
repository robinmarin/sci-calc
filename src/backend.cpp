#include "backend.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr int MaxHistory = 500;

// Values are stored as text so the full long double precision round-trips.
QString store(long double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.21Lg", v);
    return QString::fromLatin1(buffer);
}
long double restore(const QJsonValue &v) {
    const QByteArray text = v.toString().toLatin1();
    return text.isEmpty() ? 0 : std::strtold(text.constData(), nullptr);
}
} // namespace

Backend::Backend(const QString &statePath, QObject *parent)
    : QObject(parent),
      m_statePath(statePath.isEmpty()
                      ? QDir::homePath() + "/.local/state/scicalc/state.json"
                      : statePath) {
    load();
}

void Backend::setDegrees(bool degrees) {
    if (degrees == this->degrees())
        return;
    m_engine.setAngle(degrees ? Engine::Angle::Degrees : Engine::Angle::Radians);
    emit degreesChanged();
    save();
}

QString Backend::lastResult() const {
    return m_history.isEmpty() ? QString() : m_history.last().toMap().value("result").toString();
}

QVariantList Backend::variables() const {
    QVariantList list;
    const auto &vars = m_engine.variables();
    for (auto it = vars.begin(); it != vars.end(); ++it)
        list.append(QVariantMap{{"name", it.key()}, {"value", Engine::format(it.value())}});
    return list;
}

QString Backend::withAns(const QString &line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return line;
    // Not '-': "-5" is far more often a negative number than "ans-5".
    static const QString continuing = QStringLiteral("+*/^%!×÷·²³→");
    if (continuing.contains(trimmed.at(0)) || trimmed.startsWith("->"))
        return "ans" + trimmed;
    return line;
}

bool Backend::evaluate(const QString &line) {
    QString input = line.trimmed();
    // Enter on an empty line repeats the last entry, so "ans+1" counts up.
    if (input.isEmpty()) {
        if (m_history.isEmpty())
            return false;
        input = m_history.last().toMap().value("input").toString();
    }
    const QString expanded = withAns(input);
    const int shift = expanded.size() - input.size();

    const Engine::Result result = m_engine.evaluate(expanded);
    if (!result.ok) {
        setError(result.error, result.errorPosition < 0 ? -1 : std::max(0, result.errorPosition - shift));
        return false;
    }

    setError({}, -1);
    m_history.append(QVariantMap{
        {"input", expanded},
        {"result", Engine::format(result.value)},
        {"fraction", Engine::fraction(result.value)},
        {"assigned", result.assigned},
    });
    while (m_history.size() > MaxHistory)
        m_history.removeFirst();
    emit historyChanged();
    save();
    return true;
}

void Backend::clearError() { setError({}, -1); }

void Backend::clearHistory() {
    m_history.clear();
    m_engine.setAns(0);
    emit historyChanged();
    save();
}

void Backend::clearVariables() {
    m_engine.clearVariables();
    emit historyChanged();
    save();
}

void Backend::copy(const QString &text) {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void Backend::setError(const QString &message, int position) {
    if (message == m_error && position == m_errorPosition)
        return;
    m_error = message;
    m_errorPosition = position;
    emit errorChanged();
}

void Backend::load() {
    QFile file(m_statePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_engine.setAngle(root.value("angle").toString() == "deg" ? Engine::Angle::Degrees : Engine::Angle::Radians);
    m_engine.setAns(restore(root.value("ans")));
    const QJsonObject vars = root.value("variables").toObject();
    for (auto it = vars.begin(); it != vars.end(); ++it)
        if (!Engine::isReserved(it.key()))
            m_engine.setVariable(it.key(), restore(it.value()));
    for (const QJsonValue &entry : root.value("history").toArray()) {
        const QJsonObject e = entry.toObject();
        if (e.value("input").toString().isEmpty())
            continue;
        m_history.append(QVariantMap{
            {"input", e.value("input").toString()},
            {"result", e.value("result").toString()},
            {"fraction", e.value("fraction").toString()},
            {"assigned", e.value("assigned").toString()},
        });
    }
    while (m_history.size() > MaxHistory)
        m_history.removeFirst();
}

// Written beside the target and renamed into place, so a crash or a kill from
// the bar never leaves a half-written state file.
void Backend::save() const {
    QJsonObject root;
    root["angle"] = degrees() ? "deg" : "rad";
    root["ans"] = store(m_engine.ans());
    QJsonObject vars;
    const auto &variables = m_engine.variables();
    for (auto it = variables.begin(); it != variables.end(); ++it)
        vars[it.key()] = store(it.value());
    root["variables"] = vars;
    QJsonArray history;
    for (const QVariant &entry : m_history)
        history.append(QJsonObject::fromVariantMap(entry.toMap()));
    root["history"] = history;

    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    QSaveFile file(m_statePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}
