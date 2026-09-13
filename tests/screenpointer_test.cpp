// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/screenpointer.h"

#include <QEnterEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QtTest>

class IgnorePointerEntry : public QObject
{
protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        return event->type() == QEvent::Enter;
    }
};

class ScreenPointerTest : public QObject
{
    Q_OBJECT

private slots:
    void pointerEntrySelectsOutputAndClosesWindows()
    {
        const auto originalWindows = QGuiApplication::allWindows();
        QTimer enter;
        enter.setSingleShot(true);
        connect(&enter, &QTimer::timeout, this, [] {
            for (QWindow* window : QGuiApplication::allWindows()) {
                if (window->title() ==
                    QStringLiteral("Flameshot monitor detection")) {
                    QEnterEvent event(
                      QPointF(100, 100), QPointF(100, 100), QPointF(100, 100));
                    QCoreApplication::sendEvent(window, &event);
                    break;
                }
            }
        });
        enter.start(20);
        QCOMPARE(ScreenPointer::waylandScreen(),
                 QGuiApplication::primaryScreen());
        QCOMPARE(QGuiApplication::allWindows(), originalWindows);
    }

    void timeoutClosesWindows()
    {
        const auto originalWindows = QGuiApplication::allWindows();
        IgnorePointerEntry filter;
        qApp->installEventFilter(&filter);
        QElapsedTimer elapsed;
        elapsed.start();
        QCOMPARE(ScreenPointer::waylandScreen(), nullptr);
        QVERIFY(elapsed.elapsed() >= 500);
        QVERIFY(elapsed.elapsed() < 2000);
        QCOMPARE(QGuiApplication::allWindows(), originalWindows);
        qApp->removeEventFilter(&filter);
    }
};

QTEST_MAIN(ScreenPointerTest)
#include "screenpointer_test.moc"
