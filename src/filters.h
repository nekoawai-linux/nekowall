#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

// What the user is asking for, in one place: the three programs that need it
// -- the picker, the window and the headless run -- must agree, and the file
// on disk is what they agree through.
struct Filters {
	enum Mode {
		Safe, // only what the gallery marks as safe
		Adult, // only what it marks as adult
		Everything // whatever comes
	};

	// The size of the finished wallpaper. Auto is the screen the machine is
	// running; AsItComes hands the picture over untouched, for a desktop
	// that would rather scale it itself.
	enum Size { Auto, HD, FullHD, WQHD, UHD, Custom, AsItComes };

	Mode mode = Safe;
	Size size = Auto;
	QSize custom = QSize(1920, 1080);
	QStringList wanted; // any of these tags; empty means no preference
	QStringList blocked; // none of these tags, whatever the mode

	// The canvas to draw, given what the kernel reports. An invalid size
	// means "leave the picture as it is".
	QSize target(QSize screen) const;

	static Filters load();
	void save() const;

	static QString modeKey(Mode mode);
	static Mode modeFromKey(const QString &key);
	static QString sizeKey(Size size);
	static Size sizeFromKey(const QString &key);

	// The tags offered as boxes to tick. Written down rather than fetched:
	// the gallery has no endpoint that lists them, and a tag nobody drew is
	// a box that always comes back empty. Every one of these was measured
	// against the search before it got in.
	static QStringList offeredTags();

	// Ticked by default. The gallery's own safe flag lets these through.
	static QStringList defaultBlockedTags();
};
