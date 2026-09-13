// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPixmap>
#include <QRect>
#include <optional>

namespace PortalImage {

struct Mapping
{
    QRect pixels;
    qreal pixelRatio;
};

std::optional<Mapping> mapScreen(const QSize& imageSize,
                                 const QRect& desktop,
                                 const QRect& screen);
QPixmap crop(const QPixmap& image, const Mapping& mapping);
QRect toPixels(const QRect& logical, qreal pixelRatio);

}
