#include "filters.h"

#include <QSettings>

namespace {

QSettings settings()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope,
		QStringLiteral("nekowall"), QStringLiteral("nekowall"));
}

} // namespace

QString Filters::modeKey(Mode mode)
{
	switch (mode) {
	case Adult:
		return QStringLiteral("nsfw");
	case Everything:
		return QStringLiteral("all");
	case Safe:
		break;
	}
	return QStringLiteral("safe");
}

Filters::Mode Filters::modeFromKey(const QString &key)
{
	if (key == QLatin1String("nsfw"))
		return Adult;
	if (key == QLatin1String("all"))
		return Everything;
	return Safe;
}

QString Filters::sizeKey(Size size)
{
	switch (size) {
	case HD:
		return QStringLiteral("1280x720");
	case FullHD:
		return QStringLiteral("1920x1080");
	case WQHD:
		return QStringLiteral("2560x1440");
	case UHD:
		return QStringLiteral("3840x2160");
	case Custom:
		return QStringLiteral("custom");
	case AsItComes:
		return QStringLiteral("any");
	case Auto:
		break;
	}
	return QStringLiteral("auto");
}

Filters::Size Filters::sizeFromKey(const QString &key)
{
	if (key == QLatin1String("1280x720"))
		return HD;
	if (key == QLatin1String("1920x1080"))
		return FullHD;
	if (key == QLatin1String("2560x1440"))
		return WQHD;
	if (key == QLatin1String("3840x2160"))
		return UHD;
	if (key == QLatin1String("custom"))
		return Custom;
	if (key == QLatin1String("any"))
		return AsItComes;
	return Auto;
}

QSize Filters::target(QSize screen) const
{
	switch (size) {
	case HD:
		return QSize(1280, 720);
	case FullHD:
		return QSize(1920, 1080);
	case WQHD:
		return QSize(2560, 1440);
	case UHD:
		return QSize(3840, 2160);
	case Custom:
		return custom;
	case AsItComes:
		// Nothing to draw: the picture goes to the desktop as it was
		// downloaded, and scaling it is the desktop's business.
		return QSize();
	case Auto:
		break;
	}
	return screen;
}

Filters Filters::load()
{
	QSettings store = settings();
	Filters filters;
	filters.mode = modeFromKey(store.value(QStringLiteral("mode"),
					     modeKey(Safe)).toString());
	filters.size = sizeFromKey(store.value(QStringLiteral("size"),
					     sizeKey(Auto)).toString());
	filters.custom = QSize(store.value(QStringLiteral("customWidth"), 1920).toInt(),
		store.value(QStringLiteral("customHeight"), 1080).toInt());
	filters.wanted = store.value(QStringLiteral("wantedTags")).toStringList();
	// An absent list is the default one; an empty list is a user who ticked
	// every box off, and that is a different thing.
	filters.blocked = store.contains(QStringLiteral("blockedTags"))
		? store.value(QStringLiteral("blockedTags")).toStringList()
		: defaultBlockedTags();
	return filters;
}

void Filters::save() const
{
	QSettings store = settings();
	store.setValue(QStringLiteral("mode"), modeKey(mode));
	store.setValue(QStringLiteral("size"), sizeKey(size));
	store.setValue(QStringLiteral("customWidth"), custom.width());
	store.setValue(QStringLiteral("customHeight"), custom.height());
	// An empty list would be written as @Invalid(), which is Qt talking to
	// itself in a file a person is meant to be able to read.
	if (wanted.isEmpty())
		store.remove(QStringLiteral("wantedTags"));
	else
		store.setValue(QStringLiteral("wantedTags"), wanted);
	store.setValue(QStringLiteral("blockedTags"), blocked);
}

QStringList Filters::offeredTags()
{
	// Every one of these was counted against the search on 11 August 2026,
	// and only tags with pictures behind them got in: a box that always
	// comes back empty is worse than no box. Two lessons from the counting.
	// The gallery writes tags in the singular -- "flower" has a gallery's
	// worth, "flowers" has none, and the same for "star". And it is a
	// gallery of characters, not of places: "city" had one picture, "sea"
	// none, so scenery is not offered at all.
	return {
		QStringLiteral("cat ears"),
		QStringLiteral("fox ears"),
		QStringLiteral("animal ears"),
		QStringLiteral("tail"),
		QStringLiteral("cat"),
		QStringLiteral("long hair"),
		QStringLiteral("short hair"),
		QStringLiteral("twintails"),
		QStringLiteral("blue eyes"),
		QStringLiteral("smile"),
		QStringLiteral("sitting"),
		QStringLiteral("kimono"),
		QStringLiteral("school uniform"),
		QStringLiteral("dress"),
		QStringLiteral("sweater"),
		QStringLiteral("hoodie"),
		QStringLiteral("maid"),
		QStringLiteral("ribbon"),
		QStringLiteral("hat"),
		QStringLiteral("glasses"),
		QStringLiteral("flower"),
		QStringLiteral("star"),
		QStringLiteral("sky"),
		QStringLiteral("food"),
	};
}

QStringList Filters::defaultBlockedTags()
{
	return {
		QStringLiteral("cleavage"),
		QStringLiteral("breasts"),
		QStringLiteral("sideboob"),
		QStringLiteral("underboob"),
		QStringLiteral("bikini"),
		QStringLiteral("swimsuit"),
		QStringLiteral("lingerie"),
		QStringLiteral("underwear"),
		QStringLiteral("panties"),
		QStringLiteral("pantsu"),
		QStringLiteral("bra"),
		QStringLiteral("garter"),
		QStringLiteral("nude"),
		QStringLiteral("naked"),
		QStringLiteral("nipples"),
		QStringLiteral("areola"),
		QStringLiteral("topless"),
		QStringLiteral("ecchi"),
		QStringLiteral("erotic"),
		QStringLiteral("suggestive"),
		QStringLiteral("bondage"),
		QStringLiteral("see-through"),
		QStringLiteral("wet clothes"),
		QStringLiteral("undressing"),
	};
}

QStringList Filters::alwaysBlockedTags()
{
	// These never reach a screen, whatever the mode says and whatever the
	// settings file says. They are here rather than among the tick boxes
	// because a tick box is an offer, and this is not on offer.
	return {
		QStringLiteral("loli"),
		QStringLiteral("shota"),
		QStringLiteral("toddlercon"),
		QStringLiteral("child"),
	};
}
