#pragma once
#include <QIcon>
#include <QQmlApplicationEngine>

class FocusBackend;

// Loads qml/Main.qml with `backend` visible to QML as the name "backend".
// Returns the root window object, or nullptr if the QML failed to load.
QObject* loadMainWindow(QQmlApplicationEngine& engine, FocusBackend& backend);

// The app/tray icon, drawn in code so there is no image file to ship yet.
QIcon makeAppIcon();
