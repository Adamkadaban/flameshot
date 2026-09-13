// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenpointer.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QPainter>
#include <QPointer>
#include <QRasterWindow>
#include <QScreen>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

namespace {

class PointerWindow : public QRasterWindow
{
public:
    std::function<void(QScreen*)> entered;

    explicit PointerWindow(QScreen* output)
    {
        setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::WindowDoesNotAcceptFocus);
        QSurfaceFormat format;
        format.setAlphaBufferSize(8);
        setFormat(format);
        setScreen(output);
        setTitle(QStringLiteral("Flameshot monitor detection"));
        setWindowState(Qt::WindowFullScreen);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(QRect(QPoint(), size()), Qt::transparent);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter && entered) {
            entered(screen());
        }
        return QRasterWindow::event(event);
    }
};

}

QScreen* ScreenPointer::waylandScreen()
{
    QEventLoop loop;
    QPointer<QScreen> selected;
    std::vector<std::unique_ptr<PointerWindow>> windows;

    // Wayland reports pointer entry on our own surfaces, not global cursor
    // coordinates. Each transparent surface is assigned to an explicit output.
    for (QScreen* screen : QGuiApplication::screens()) {
        auto window = std::make_unique<PointerWindow>(screen);
        window->entered = [&](QScreen* output) {
            if (!selected) {
                selected = output;
                loop.quit();
            }
        };
        windows.push_back(std::move(window));
    }

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(
      qGuiApp, &QGuiApplication::screenRemoved, &loop, &QEventLoop::quit);
    timeout.start(750);
    for (const auto& window : windows) {
        window->setVisible(true);
    }
    if (!selected) {
        loop.exec();
    }
    windows.clear();
    return selected.data();
}
