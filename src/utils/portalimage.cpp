// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalimage.h"

#include <QImageReader>
#include <QPromise>
#include <QThreadPool>
#include <QtMath>
#include <memory>

PortalImage::ReadResult PortalImage::read(const QString& path)
{
    QImageReader reader(path);
    QImage image = reader.read();
    QString error = image.isNull() ? reader.errorString() : QString();
    return { std::move(image), error };
}

QFuture<PortalImage::ReadResult> PortalImage::readAsync(const QString& path)
{
    // Decode QImage off-thread; callers create QPixmap on the GUI thread.
    auto promise = std::make_shared<QPromise<ReadResult>>();
    promise->start();
    auto future = promise->future();
    QThreadPool::globalInstance()->start([promise, path] {
        promise->addResult(read(path));
        promise->finish();
    });
    return future;
}

std::optional<PortalImage::Mapping> PortalImage::mapScreen(
  const QSize& imageSize,
  const QRect& desktop,
  const QRect& screen)
{
    if (imageSize.isEmpty() || desktop.isEmpty() || screen.isEmpty() ||
        !desktop.contains(screen)) {
        return std::nullopt;
    }

    const qreal scaleX = qreal(imageSize.width()) / desktop.width();
    const qreal scaleY = qreal(imageSize.height()) / desktop.height();
    // A uniformly scaled image can round either canvas dimension by a pixel.
    const qreal tolerance = qMax(1.0 / desktop.width(), 1.0 / desktop.height());
    if (qAbs(scaleX - scaleY) > tolerance) {
        return std::nullopt;
    }

    const QRect local = screen.translated(-desktop.topLeft());
    const int left = qRound(local.x() * scaleX);
    const int top = qRound(local.y() * scaleY);
    const int right = qRound((local.x() + local.width()) * scaleX);
    const int bottom = qRound((local.y() + local.height()) * scaleY);
    return Mapping{ QRect(left, top, right - left, bottom - top), scaleX };
}

QPixmap PortalImage::crop(const QPixmap& image, const Mapping& mapping)
{
    QPixmap result = image.copy(mapping.pixels);
    // Screenshot pixels and the window's rendering buffer have independent
    // scales on fractional-scale Wayland outputs. Do not resample the image.
    result.setDevicePixelRatio(mapping.pixelRatio);
    return result;
}

QRect PortalImage::toPixels(const QRect& logical, qreal pixelRatio)
{
    const int left = qRound(logical.x() * pixelRatio);
    const int top = qRound(logical.y() * pixelRatio);
    const int right = qRound((logical.x() + logical.width()) * pixelRatio);
    const int bottom = qRound((logical.y() + logical.height()) * pixelRatio);
    return QRect(left, top, right - left, bottom - top);
}
