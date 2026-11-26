#include <QAction>
#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSystemTrayIcon>

#include "FocusBackend.h"
#include "UiApp.h"
#include "core/ActivitySource.h"

#ifdef _WIN32
#include "platform/Paths.h"
#include "platform/WindowsActivitySource.h"
#endif

int runSelfTest(QApplication& app, const QString& screenshotDir);

int main(int argc, char* argv[]) {
    // Qt Quick Controls: use the "Basic" style, the one that lets us restyle everything.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QApplication app(argc, argv);  // QApplication (not QGuiApplication) because the tray icon is a widget class
    QApplication::setApplicationName(QStringLiteral("Focus"));
    QApplication::setOrganizationName(QStringLiteral("Focus"));
    QApplication::setWindowIcon(makeAppIcon());

    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--self-test")) {
            return runSelfTest(app, i + 1 < argc ? QString::fromLocal8Bit(argv[i + 1]) : QString());
        }
    }

    // Same database file as the console app, so both front ends share your history.
#ifdef _WIN32
    const std::string dir = focus::win::appDataDirectory();
    if (dir.empty()) return 1;
    const std::string dbPath = dir + "\\focus.db";
    auto source = std::make_unique<focus::win::WindowsActivitySource>();
#else
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const std::string dbPath = (dir + QStringLiteral("/focus.db")).toStdString();
    auto source = std::make_unique<focus::NullActivitySource>();  // no window detection on this platform
#endif

    try {
        FocusBackend backend(dbPath, std::move(source));
        QQmlApplicationEngine engine;
        auto* window = qobject_cast<QQuickWindow*>(loadMainWindow(engine, backend));
        if (!window) return 2;

        // Tray icon: keeps the session alive when the window is closed, and shows notifications.
        QSystemTrayIcon tray(makeAppIcon());
        QMenu menu;
        auto* openAction   = menu.addAction(QStringLiteral("Open Focus"));
        menu.addSeparator();
        auto* pauseAction  = menu.addAction(QStringLiteral("Pause"));
        auto* waterAction  = menu.addAction(QStringLiteral("Log a glass of water"));
        auto* aiAction     = menu.addAction(QStringLiteral("Log an AI use"));
        auto* endAction    = menu.addAction(QStringLiteral("End session"));
        menu.addSeparator();
        auto* quitAction   = menu.addAction(QStringLiteral("Quit"));

        const bool hasTray = QSystemTrayIcon::isSystemTrayAvailable();
        QApplication::setQuitOnLastWindowClosed(!hasTray);  // with a tray, closing the window keeps running

        auto showWindow = [window] { window->show(); window->raise(); window->requestActivate(); };
        QObject::connect(openAction, &QAction::triggered, showWindow);
        QObject::connect(&tray, &QSystemTrayIcon::activated, [showWindow](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) showWindow();
        });
        QObject::connect(pauseAction, &QAction::triggered, [&backend] {
            if (backend.phase() == QLatin1String("paused")) backend.resume(); else backend.pause();
        });
        QObject::connect(waterAction, &QAction::triggered, &backend, &FocusBackend::logWater);
        QObject::connect(aiAction, &QAction::triggered, &backend, &FocusBackend::logAiUse);
        QObject::connect(endAction, &QAction::triggered, &backend, &FocusBackend::abandon);
        QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

        // Menu entries follow the session state.
        QObject::connect(&backend, &FocusBackend::changed, [&] {
            const QString phase = backend.phase();
            const bool active   = phase == QLatin1String("focusing") || phase == QLatin1String("paused");
            pauseAction->setEnabled(active);
            pauseAction->setText(phase == QLatin1String("paused") ? QStringLiteral("Resume") : QStringLiteral("Pause"));
            aiAction->setEnabled(active);
            endAction->setEnabled(active);
        });
        emit backend.changed();

        QObject::connect(&backend, &FocusBackend::notification, [&](const QString& title, const QString& body) {
            if (hasTray) tray.showMessage(title, body, QSystemTrayIcon::Information, 6000);
        });
        QObject::connect(&backend, &FocusBackend::trackingChanged, [&] {
            const QString mode = backend.trackingMode();
            tray.setToolTip(QStringLiteral("Focus - tracking: ") + (mode == QLatin1String("work") ? QStringLiteral("full detail")
                                                                  : mode == QLatin1String("private") ? QStringLiteral("category only")
                                                                                                     : QStringLiteral("off")));
        });
        emit backend.trackingChanged();
        QObject::connect(&app, &QApplication::aboutToQuit, [&backend] { backend.shutdown(); });

        tray.setContextMenu(&menu);
        tray.setToolTip(QStringLiteral("Focus"));
        if (hasTray) tray.show();

        return app.exec();
    } catch (const focus::StorageError& e) {
        qCritical("Storage error: %s", e.what());
        return 3;
    }
}
