// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <array>
#include <memory>
#include <vector>

class ReferenceWindow : public QWidget
{
public:
    explicit ReferenceWindow(QScreen* output)
    {
        setWindowTitle(
          QStringLiteral("Flameshot capture regression reference"));
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint |
                       Qt::FramelessWindowHint);
        setScreen(output);
        move(output->geometry().topLeft());
        resize(output->geometry().size());
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const std::array<QColor, 3> colors{ Qt::red, Qt::green, Qt::blue };
        for (int i = 0; i < 3; ++i) {
            const int left = width() * i / 3;
            const int right = width() * (i + 1) / 3;
            painter.fillRect(QRect(left, 0, right - left, height()), colors[i]);
        }
        painter.setPen(Qt::white);
        painter.drawText(
          30,
          40,
          QStringLiteral(
            "Real Wayland capture regression check - closes within 5 seconds"));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    const bool referenceOnly =
      app.arguments().size() == 2 &&
      app.arguments().at(1) == QStringLiteral("--reference-only");
    if ((!referenceOnly && app.arguments().size() != 3) ||
        !app.platformName().startsWith(QLatin1String("wayland"))) {
        qCritical() << "Usage: QT_QPA_PLATFORM=wayland"
                    << app.arguments().first()
                    << "/path/to/flameshot /path/to/result.png";
        return 2;
    }

    std::vector<std::unique_ptr<ReferenceWindow>> windows;
    for (QScreen* screen : app.screens()) {
        auto window = std::make_unique<ReferenceWindow>(screen);
        window->showFullScreen();
        windows.push_back(std::move(window));
    }
    if (referenceOnly) {
        QTimer::singleShot(5000, &app, &QApplication::quit);
        return app.exec();
    }

    QProcess capture;
    bool finished = false;
    const auto finish = [&](int status) {
        if (finished)
            return;
        finished = true;
        if (capture.state() != QProcess::NotRunning) {
            capture.kill();
            capture.waitForFinished(1000);
        }
        app.closeAllWindows();
        app.exit(status);
    };
    QObject::connect(&capture, &QProcess::errorOccurred, &app, [&] {
        qCritical() << "Capture process failed:" << capture.errorString();
        finish(1);
    });
    QObject::connect(
      &capture,
      &QProcess::finished,
      &app,
      [&](int status, QProcess::ExitStatus exitStatus) {
          if (finished)
              return;
          const QImage image =
            QImage::fromData(capture.readAllStandardOutput(), "PNG");
          if (status != 0 || exitStatus != QProcess::NormalExit ||
              image.isNull()) {
              qCritical() << "Capture failed:"
                          << capture.readAllStandardError();
              finish(1);
              return;
          }
          if (!image.save(app.arguments().at(2), "PNG")) {
              qCritical() << "Could not save capture evidence";
              finish(1);
              return;
          }
          std::array<int, 3> counts{};
          constexpr int samples = 60;
          for (int y = 0; y < samples; ++y) {
              for (int x = 0; x < samples; ++x) {
                  const QColor color = image.pixelColor(
                    (2 * x + 1) * image.width() / (2 * samples),
                    (2 * y + 1) * image.height() / (2 * samples));
                  if (color.alpha() < 250)
                      continue;
                  const std::array<int, 3> channels{ color.red(),
                                                     color.green(),
                                                     color.blue() };
                  for (int channel = 0; channel < 3; ++channel) {
                      if (channels[channel] > 220 &&
                          channels[(channel + 1) % 3] < 30 &&
                          channels[(channel + 2) % 3] < 30) {
                          ++counts[channel];
                      }
                  }
              }
          }
          qInfo() << "Real automatic capture:" << image.size()
                  << "reference-color samples:" << counts[0] << counts[1]
                  << counts[2];
          for (int count : counts) {
              if (count < samples * samples / 6) {
                  qCritical()
                    << "The capture is black or does not contain the reference";
                  finish(1);
                  return;
              }
          }
          finish(0);
      });
    QTimer::singleShot(300, &app, [&] {
        for (const auto& window : windows) {
            qInfo() << "Reference:" << window->screen()->name()
                    << window->geometry() << "visible" << window->isVisible();
        }
        capture.start(app.arguments().at(1), { "screen", "--raw" });
    });
    QTimer::singleShot(5000, &app, [&] {
        qCritical()
          << "Capture timed out; enable Capture monitor under pointer"
             " and approve screenshot portal access before this check";
        finish(1);
    });
    return app.exec();
}
