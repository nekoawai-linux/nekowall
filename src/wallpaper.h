#pragma once

#include <QImage>
#include <QSize>
#include <QString>

struct Artwork;

namespace wallpaper {

// What the machine is actually driving, taken from the kernel rather than
// from the session: the answer is the same under X11, under Wayland and with
// no session at all.
QSize screenSize();

// A canvas of exactly the screen size. A picture of nearly the right shape is
// scaled to cover it; anything else is laid on a blurred, darkened copy of
// itself, whole and uncut. `cropped` reports which of the two happened.
QImage compose(const QImage &art, QSize screen, bool *cropped = nullptr);

// Writes the canvas and what is known about the picture next to it. The file
// name carries the picture id: desktops cache wallpapers by path, and a new
// picture under an old name is a wallpaper that never changes.
QString store(const QImage &canvas, const Artwork &art, bool cropped, QString *error);

// Hands the file to whatever desktop is running.
bool apply(const QString &path, QString *error);

QString desktopName();
QString stateDirectory();
QString describeCurrent();

} // namespace wallpaper
