#pragma once

#include <QObject>
#include <QVariantList>

#include "engine.h"

// The calculator as QML sees it: a history of entries, the angle mode, and the
// last error. Everything is saved after each change, so a calculator closed
// from the bar comes back exactly as it was.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool degrees READ degrees WRITE setDegrees NOTIFY degreesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int errorPosition READ errorPosition NOTIFY errorChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY historyChanged)
    Q_PROPERTY(QVariantList variables READ variables NOTIFY historyChanged)

public:
    // An empty statePath means ~/.local/state/scicalc/state.json; tests pass
    // a temporary file.
    explicit Backend(const QString &statePath = {}, QObject *parent = nullptr);

    QVariantList history() const { return m_history; }
    bool degrees() const { return m_engine.angle() == Engine::Angle::Degrees; }
    void setDegrees(bool degrees);
    QString error() const { return m_error; }
    int errorPosition() const { return m_errorPosition; }
    QString lastResult() const;
    QVariantList variables() const;

    // Evaluates a line and appends it to the history. Returns false and sets
    // error when it fails; the history is left alone then.
    Q_INVOKABLE bool evaluate(const QString &line);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void clearVariables();
    Q_INVOKABLE void copy(const QString &text);
    Q_INVOKABLE QString grouped(const QString &number) const { return Engine::groupThousands(number); }
    Q_INVOKABLE QStringList functionNames() const { return Engine::functionNames(); }

    // A line starting with an operator continues from the last answer, as on
    // a TI: "*2" means "ans*2".
    static QString withAns(const QString &line);

    Engine &engine() { return m_engine; }

signals:
    void historyChanged();
    void degreesChanged();
    void errorChanged();

private:
    void load();
    void save() const;
    void setError(const QString &message, int position);

    QString m_statePath;
    Engine m_engine;
    QVariantList m_history;
    QString m_error;
    int m_errorPosition = -1;
};
