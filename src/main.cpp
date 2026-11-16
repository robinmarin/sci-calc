// scicalc — a keyboard-first scientific calculator for one-off sums.
//
//   scicalc                 open the calculator, or focus it if already open
//   scicalc toggle          open it, or close it if it is open (the bar button)
//   scicalc [--deg] [--json] <expression>
//                           print the answer without opening a window

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QUrl>
#include <unistd.h>

#include "backend.h"
#include "engine.h"
#include "theme.h"
#include "translator.h"

namespace {

const char *Version = "0.1.0";

QString serverName() { return QString("scicalc-%1").arg(getuid()); }

void printUsage(QTextStream &out) {
    out << "Usage:\n"
           "  scicalc                         open the calculator (focuses it if open)\n"
           "  scicalc toggle                  open it, or close it if it is open\n"
           "  scicalc [--deg] [--json] EXPR   print the answer and exit\n"
           "\n"
           "Examples:\n"
           "  scicalc '2^10'\n"
           "  scicalc --deg 'sin(30) + 1'\n"
           "  scicalc '0xff + 0b101'\n";
}

// One-off answers in a terminal or from a script, without a display.
int runCli(const QStringList &args) {
    QTextStream out(stdout), err(stderr);
    bool json = false;
    Engine engine;
    QStringList words;
    for (const QString &arg : args) {
        if (arg == "--json") json = true;
        else if (arg == "--deg") engine.setAngle(Engine::Angle::Degrees);
        else if (arg == "--rad") engine.setAngle(Engine::Angle::Radians);
        else words << arg;
    }
    const QString expression = words.join(' ');
    const Engine::Result result = engine.evaluate(expression);

    if (json) {
        QJsonObject object{{"ok", result.ok}, {"input", expression}};
        if (result.ok) {
            object["result"] = Engine::format(result.value);
            const QString fraction = Engine::fraction(result.value);
            if (!fraction.isEmpty()) object["fraction"] = fraction;
        } else {
            object["error"] = result.error;
            object["position"] = result.errorPosition;
        }
        (result.ok ? out : err) << QJsonDocument(object).toJson(QJsonDocument::Compact) << "\n";
    } else if (result.ok) {
        out << Engine::format(result.value) << "\n";
    } else {
        err << "scicalc: " << result.error << "\n";
    }
    return result.ok ? 0 : 1;
}

// Passes a message to a calculator that is already running. Returns true when
// one answered, so this process can leave.
bool tellRunning(const QByteArray &message) {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(200))
        return false;
    socket.write(message);
    socket.flush();
    socket.waitForBytesWritten(200);
    socket.disconnectFromServer();
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    if (!args.isEmpty() && (args.first() == "--help" || args.first() == "-h")) {
        QTextStream out(stdout);
        printUsage(out);
        return 0;
    }
    if (!args.isEmpty() && args.first() == "--version") {
        QTextStream(stdout) << "scicalc " << Version << "\n";
        return 0;
    }

    const bool toggle = !args.isEmpty() && args.first() == "toggle";
    const bool open = args.isEmpty() || toggle || args.first() == "open";
    if (!open) {
        QCoreApplication app(argc, argv);
        Translator translator;
        app.installTranslator(&translator);
        return runCli(args);
    }

    if (tellRunning(toggle ? "quit" : "show"))
        return 0;

    QGuiApplication app(argc, argv);
    app.setApplicationName("scicalc");
    app.setApplicationVersion(Version);

    // Matches the window to scicalc.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon, and the Hyprland
    // rule that floats it applies.
    app.setDesktopFileName("scicalc");
    app.setWindowIcon(QIcon::fromTheme("scicalc"));

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // A second launch either closes this one (toggle) or brings it forward.
    QLocalServer::removeServer(serverName());
    QLocalServer server;
    server.listen(serverName());
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &engine] {
                const QByteArray message = socket->readAll().trimmed();
                if (message == "quit") {
                    QCoreApplication::quit();
                } else if (message == "show") {
                    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
                        window->show();
                        window->raise();
                        window->requestActivate();
                    }
                    // Hyprland ignores activation requests without a token,
                    // so ask it directly when it is the compositor.
                    if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
                        QProcess::startDetached("hyprctl", {"dispatch", "focuswindow", "class:^(scicalc)$"});
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    return app.exec();
}
