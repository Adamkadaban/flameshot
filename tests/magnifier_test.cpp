// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgets/capture/magnifierwidget.h"

#include <QCursor>
#include <QImage>
#include <QtTest>

class MagnifierTest : public QObject
{
    Q_OBJECT

private slots:
    void sampleScreenshotPixels_data()
    {
        QTest::addColumn<qreal>("ratio");
        QTest::addColumn<bool>("square");
        QTest::newRow("circle-100") << qreal(1) << false;
        QTest::newRow("circle-150") << qreal(1.5) << false;
        QTest::newRow("circle-200") << qreal(2) << false;
        QTest::newRow("square-100") << qreal(1) << true;
        QTest::newRow("square-150") << qreal(1.5) << true;
        QTest::newRow("square-200") << qreal(2) << true;
    }

    void sampleScreenshotPixels()
    {
        QFETCH(qreal, ratio);
        QFETCH(bool, square);
        QImage image(
          qRound(640 * ratio), qRound(480 * ratio), QImage::Format_RGB32);
        image.fill(Qt::white);
        const int center = qRound(100 * ratio);
        image.setPixelColor(center, center, Qt::red);
        image.setPixelColor(center - 1, center - 1, Qt::green);
        QPixmap screenshot = QPixmap::fromImage(image);
        screenshot.setDevicePixelRatio(ratio);

        QWidget parent;
        parent.resize(640, 480);
        MagnifierWidget magnifier(screenshot, Qt::blue, square, &parent);
        parent.show();
        magnifier.show();
        QCursor::setPos(parent.mapToGlobal(QPoint(100, 100)));
        QCoreApplication::processEvents();

        const QImage rendered = magnifier.grab().toImage();
        QCOMPARE(rendered.pixelColor(201, 201), QColor(Qt::red));
        QCOMPARE(rendered.pixelColor(191, 191), QColor(Qt::green));
        QCOMPARE(screenshot.devicePixelRatio(), ratio);
    }
};

QTEST_MAIN(MagnifierTest)
#include "magnifier_test.moc"
