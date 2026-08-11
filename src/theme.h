#pragma once

#include <QString>

// The look of the window. It is drawn here rather than left to whatever
// widget style the desktop has, because nekowall runs on five of them and a
// wallpaper picker that looks like a control panel on one and like a form on
// another belongs to none of them. Dark, quiet, one accent, and the picture
// is the only thing with any colour of its own.
QString nekowallStyleSheet();
