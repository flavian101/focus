#include "UiApp.h"

#include <QPainter>
#include <QPixmap>
#include <QQmlContext>
#include <QUrl>

#include "FocusBackend.h"

QObject* loadMainWindow(QQmlApplicationEngine& engine, FocusBackend& backend) {
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));  // lets QML files say "import FocusUi"
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/FocusUi/qml/Main.qml")));
    return engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().first();
}

QIcon makeAppIcon() {
    QPixmap pixmap(128, 128);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x2A, 0x2F, 0x42), 14));
    p.drawEllipse(QRectF(16, 16, 96, 96));
    p.setPen(QPen(QColor(0x7C, 0x5C, 0xFF), 14, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(16, 16, 96, 96), 90 * 16, -250 * 16);  // a ring that is about 70% complete
    return QIcon(pixmap);
}
