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

QString Filters::galleryKey(Provider provider)
{
	switch (provider) {
	case Provider::Safebooru:
		return QStringLiteral("safebooru");
	case Provider::Danbooru:
		return QStringLiteral("danbooru");
	case Provider::NekosMoe:
		break;
	}
	return QStringLiteral("nekos.moe");
}

Provider Filters::galleryFromKey(const QString &key)
{
	if (key == QLatin1String("safebooru"))
		return Provider::Safebooru;
	if (key == QLatin1String("danbooru"))
		return Provider::Danbooru;
	return Provider::NekosMoe;
}

QVector<Provider> Filters::allGalleries()
{
	return { Provider::NekosMoe, Provider::Safebooru, Provider::Danbooru };
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

QVector<Provider> Filters::providers() const
{
	QVector<Provider> chosen;
	for (Provider provider : allGalleries()) {
		if (galleries.contains(galleryKey(provider)))
			chosen << provider;
	}
	// Every gallery turned off is not a wish anybody has; it is a window
	// that would answer nothing at all.
	if (chosen.isEmpty())
		chosen << Provider::NekosMoe;
	return chosen;
}

QStringList Filters::tagsFor(Provider provider) const
{
	QStringList out;
	for (const TagChoice &choice : tagTable()) {
		if (!wanted.contains(choice.label))
			continue;
		const QString name = provider == Provider::NekosMoe ? choice.nekos : choice.booru;
		if (!name.isEmpty())
			out << name;
	}
	return out;
}

Filters Filters::load()
{
	QSettings store = settings();
	Filters filters;
	filters.mode = modeFromKey(store.value(QStringLiteral("mode"),
					     modeKey(Safe)).toString());
	filters.size = sizeFromKey(store.value(QStringLiteral("size"),
					     sizeKey(Auto)).toString());
	filters.galleries = store.value(QStringLiteral("galleries"),
					     QStringList { galleryKey(Provider::NekosMoe) })
				    .toStringList();
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
	store.setValue(QStringLiteral("galleries"), galleries);
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

QVector<TagChoice> Filters::tagTable()
{
	// What each gallery calls the same thing. nekos.moe writes tags in the
	// singular and with spaces; the boorus share the Danbooru vocabulary,
	// which is underscored and often more specific -- "ocean" rather than
	// "sea", "cityscape" rather than "city".
	//
	// The nekos.moe column was counted against it on 11 August 2026 and only
	// tags with pictures behind them got in: it is a gallery of characters,
	// so scenery is left empty there and comes from the boorus instead.
	// Explicit tags are offered by neither -- the mode above already says
	// whether adult pictures are wanted.
	return {
		{ QStringLiteral("cat ears"), QStringLiteral("cat ears"),
			QStringLiteral("cat_ears") },
		{ QStringLiteral("fox ears"), QStringLiteral("fox ears"),
			QStringLiteral("fox_ears") },
		{ QStringLiteral("animal ears"), QStringLiteral("animal ears"),
			QStringLiteral("animal_ears") },
		{ QStringLiteral("tail"), QStringLiteral("tail"),
			QStringLiteral("tail") },
		{ QStringLiteral("cat"), QStringLiteral("cat"),
			QStringLiteral("cat") },
		{ QStringLiteral("long hair"), QStringLiteral("long hair"),
			QStringLiteral("long_hair") },
		{ QStringLiteral("short hair"), QStringLiteral("short hair"),
			QStringLiteral("short_hair") },
		{ QStringLiteral("twintails"), QStringLiteral("twintails"),
			QStringLiteral("twintails") },
		{ QStringLiteral("blue eyes"), QStringLiteral("blue eyes"),
			QStringLiteral("blue_eyes") },
		{ QStringLiteral("smile"), QStringLiteral("smile"),
			QStringLiteral("smile") },
		{ QStringLiteral("sitting"), QStringLiteral("sitting"),
			QStringLiteral("sitting") },
		{ QStringLiteral("kimono"), QStringLiteral("kimono"),
			QStringLiteral("kimono") },
		{ QStringLiteral("uniform"), QStringLiteral("school uniform"),
			QStringLiteral("school_uniform") },
		{ QStringLiteral("maid"), QStringLiteral("maid"),
			QStringLiteral("maid") },
		{ QStringLiteral("dress"), QStringLiteral("dress"),
			QStringLiteral("dress") },
		{ QStringLiteral("sweater"), QStringLiteral("sweater"),
			QStringLiteral("sweater") },
		{ QStringLiteral("hoodie"), QStringLiteral("hoodie"),
			QStringLiteral("hoodie") },
		{ QStringLiteral("ribbon"), QStringLiteral("ribbon"),
			QStringLiteral("ribbon") },
		{ QStringLiteral("hat"), QStringLiteral("hat"),
			QStringLiteral("hat") },
		{ QStringLiteral("glasses"), QStringLiteral("glasses"),
			QStringLiteral("glasses") },
		{ QStringLiteral("flower"), QStringLiteral("flower"),
			QStringLiteral("flower") },
		{ QStringLiteral("food"), QStringLiteral("food"),
			QStringLiteral("food") },
		{ QStringLiteral("sky"), QStringLiteral("sky"),
			QStringLiteral("sky") },
		{ QStringLiteral("scenery"), {},
			QStringLiteral("scenery") },
		{ QStringLiteral("night"), {},
			QStringLiteral("night") },
		{ QStringLiteral("starry sky"), {},
			QStringLiteral("starry_sky") },
		{ QStringLiteral("cherry blossoms"), {},
			QStringLiteral("cherry_blossoms") },
		{ QStringLiteral("snow"), {},
			QStringLiteral("snow") },
		{ QStringLiteral("rain"), {},
			QStringLiteral("rain") },
		{ QStringLiteral("ocean"), {},
			QStringLiteral("ocean") },
		{ QStringLiteral("forest"), {},
			QStringLiteral("forest") },
		{ QStringLiteral("cityscape"), {},
			QStringLiteral("cityscape") },
		{ QStringLiteral("one piece"), QStringLiteral("one piece"),
			QStringLiteral("one_piece") },
		{ QStringLiteral("rem"), QStringLiteral("rem"),
			QStringLiteral("rem_(re:zero)") },
		{ QStringLiteral("genshin impact"), {},
			QStringLiteral("genshin_impact") },
		{ QStringLiteral("raiden shogun"), {},
			QStringLiteral("raiden_shogun") },
		{ QStringLiteral("marin kitagawa"), {},
			QStringLiteral("kitagawa_marin") },
	};
}

QStringList Filters::defaultBlockedTags()
{
	return {
		QStringLiteral("cleavage"),
		QStringLiteral("breasts"),
		QStringLiteral("oppai"),
		QStringLiteral("sideboob"),
		QStringLiteral("underboob"),
		QStringLiteral("ass"),
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
		QStringLiteral("ero"),
		QStringLiteral("hentai"),
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
	//
	// The boorus do have tags of this kind, and this is where they are
	// stopped: every picture from every gallery is measured against this
	// list before anything else, with underscores read as spaces so that a
	// booru's spelling cannot slip past. A floor, not a list.
	return {
		QStringLiteral("loli"),
		QStringLiteral("shota"),
		QStringLiteral("toddlercon"),
		QStringLiteral("child"),
		QStringLiteral("underage"),
	};
}
