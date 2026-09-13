// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/portalimage.h"

#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QtTest>

class PortalImageTest : public QObject
{
    Q_OBJECT

private slots:
    void readImage_data()
    {
        QTest::addColumn<bool>("asynchronous");
        QTest::newRow("synchronous") << false;
        QTest::newRow("asynchronous") << true;
    }

    void readImage()
    {
        QFETCH(bool, asynchronous);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("capture.png");
        QImage source(80, 60, QImage::Format_ARGB32);
        source.fill(QColor(12, 34, 56, 128));
        source.setPixelColor(20, 30, Qt::red);
        QVERIFY(source.save(path, "PNG"));
        auto result = asynchronous ? PortalImage::readAsync(path).takeResult()
                                   : PortalImage::read(path);
        QVERIFY2(!result.image.isNull(), qPrintable(result.error));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.image.size(), source.size());
        QCOMPARE(result.image.pixelColor(0, 0), source.pixelColor(0, 0));
        QCOMPARE(result.image.pixelColor(20, 30), source.pixelColor(20, 30));
    }

    void reportReadFailure_data() { readImage_data(); }

    void reportReadFailure()
    {
        QFETCH(bool, asynchronous);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("missing.png");
        auto result = asynchronous ? PortalImage::readAsync(path).takeResult()
                                   : PortalImage::read(path);
        QVERIFY(result.image.isNull());
        QVERIFY(!result.error.isEmpty());
    }

    void screenMapping_data()
    {
        QTest::addColumn<QSize>("image");
        QTest::addColumn<QRect>("desktop");
        QTest::addColumn<QRect>("screen");
        QTest::addColumn<QRect>("pixels");
        QTest::addColumn<qreal>("ratio");

        QTest::newRow("gnome-150-external")
          << QSize(6720, 2160) << QRect(0, 0, 4480, 1440)
          << QRect(1920, 0, 2560, 1440) << QRect(2880, 0, 3840, 2160)
          << qreal(1.5);
        QTest::newRow("gnome-100-laptop-on-mixed-canvas")
          << QSize(6720, 2160) << QRect(0, 0, 4480, 1440)
          << QRect(0, 360, 1920, 1080) << QRect(0, 540, 2880, 1620)
          << qreal(1.5);
        QTest::newRow("negative-origin")
          << QSize(7680, 2160) << QRect(-1920, -1080, 3840, 1080)
          << QRect(-1920, -1080, 1920, 1080) << QRect(0, 0, 3840, 2160)
          << qreal(2);
        QTest::newRow("positive-origin")
          << QSize(2400, 1200) << QRect(1000, 500, 1600, 800)
          << QRect(1800, 500, 800, 800) << QRect(1200, 0, 1200, 1200)
          << qreal(1.5);
        QTest::newRow("portrait")
          << QSize(1080, 1920) << QRect(0, 0, 720, 1280)
          << QRect(0, 0, 720, 1280) << QRect(0, 0, 1080, 1920) << qreal(1.5);
        QTest::newRow("single-unscaled")
          << QSize(1920, 1080) << QRect(0, 0, 1920, 1080)
          << QRect(0, 0, 1920, 1080) << QRect(0, 0, 1920, 1080) << qreal(1);
        QTest::newRow("fractional-rounded-edges")
          << QSize(15, 9) << QRect(0, 0, 10, 6) << QRect(1, 1, 3, 3)
          << QRect(2, 2, 4, 4) << qreal(1.5);
    }

    void screenMapping()
    {
        QFETCH(QSize, image);
        QFETCH(QRect, desktop);
        QFETCH(QRect, screen);
        QFETCH(QRect, pixels);
        QFETCH(qreal, ratio);
        const auto mapping = PortalImage::mapScreen(image, desktop, screen);
        QVERIFY(mapping.has_value());
        QCOMPARE(mapping->pixels, pixels);
        QCOMPARE(mapping->pixelRatio, ratio);
    }

    void rejectInvalidMapping()
    {
        QVERIFY(!PortalImage::mapScreen(
          {}, QRect(0, 0, 100, 100), QRect(0, 0, 100, 100)));
        QVERIFY(
          !PortalImage::mapScreen(QSize(100, 100), {}, QRect(0, 0, 100, 100)));
        QVERIFY(
          !PortalImage::mapScreen(QSize(100, 100), QRect(0, 0, 100, 100), {}));
        QVERIFY(!PortalImage::mapScreen(
          QSize(100, 100), QRect(0, 0, 100, 100), QRect(-1, 0, 100, 100)));
        QVERIFY(!PortalImage::mapScreen(
          QSize(200, 100), QRect(0, 0, 100, 100), QRect(0, 0, 100, 100)));
    }

    void preservePixelsAndAnnotations()
    {
        QImage source(150, 90, QImage::Format_RGB32);
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                source.setPixelColor(x, y, (x + y) % 2 ? Qt::black : Qt::white);
            }
        }

        const auto mapping = PortalImage::mapScreen(
          source.size(), QRect(0, 0, 100, 60), QRect(20, 0, 80, 60));
        QVERIFY(mapping.has_value());
        QPixmap cropped =
          PortalImage::crop(QPixmap::fromImage(source), *mapping);
        QCOMPARE(cropped.size(), QSize(120, 90));
        QCOMPARE(cropped.devicePixelRatio(), qreal(1.5));
        QImage actual = cropped.toImage();
        actual.setDevicePixelRatio(1);
        QCOMPARE(actual, source.copy(mapping->pixels));

        {
            QPainter painter(&cropped);
            painter.fillRect(QRect(10, 10, 20, 20), Qt::red);
        }
        QCOMPARE(cropped.toImage().pixelColor(15, 15), QColor(Qt::red));
        QCOMPARE(cropped.toImage().pixelColor(44, 44), QColor(Qt::red));
        QCOMPARE(cropped.toImage().pixelColor(45, 45),
                 source.pixelColor(75, 45));

        QByteArray png;
        QBuffer buffer(&png);
        QVERIFY(cropped.save(&buffer, "PNG"));
        const QImage restored = QImage::fromData(png, "PNG");
        QCOMPARE(restored.size(), cropped.size());
        QCOMPARE(restored.pixelColor(44, 44), QColor(Qt::red));
    }

    void adjacentSelectionsShareEdge()
    {
        const QRect first = PortalImage::toPixels(QRect(1, 1, 3, 3), 1.5);
        const QRect second = PortalImage::toPixels(QRect(4, 1, 3, 3), 1.5);
        QCOMPARE(first, QRect(2, 2, 4, 4));
        QCOMPARE(first.x() + first.width(), second.x());
        QCOMPARE(PortalImage::toPixels(QRect(0, 0, 2560, 1440), 1.5),
                 QRect(0, 0, 3840, 2160));
    }
};

QTEST_MAIN(PortalImageTest)
#include "portalimage_test.moc"
