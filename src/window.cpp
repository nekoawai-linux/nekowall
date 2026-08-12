#include "window.h"

#include "wallpaper.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

const int kTagColumns = 4;

QPushButton *pill(const QString &text, QWidget *parent, const char *kind = "true")
{
	QPushButton *button = new QPushButton(text, parent);
	button->setCheckable(true);
	button->setCursor(Qt::PointingHandCursor);
	button->setProperty("pill", QString::fromLatin1(kind));
	return button;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
	QLabel *label = new QLabel(text, parent);
	label->setObjectName(QStringLiteral("section"));
	return label;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
	QLabel *label = new QLabel(text, parent);
	label->setObjectName(QStringLiteral("hint"));
	label->setWordWrap(true);
	return label;
}

// A row where one pill stays pressed and pressing another releases the rest.
// Both tabs are built out of these, so it belongs to neither.
QWidget *pillRow(QWidget *parent, const QVector<QPair<int, QString>> &choices, int current,
	const std::function<void(int)> &chosen, int columns)
{
	QWidget *holder = new QWidget(parent);
	QGridLayout *grid = new QGridLayout(holder);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(6);

	QVector<QPushButton *> buttons;
	for (int i = 0; i < choices.size(); ++i) {
		QPushButton *button = pill(choices.at(i).second, holder);
		button->setChecked(choices.at(i).first == current);
		grid->addWidget(button, i / columns, i % columns);
		buttons << button;
	}
	for (int i = 0; i < buttons.size(); ++i) {
		QPushButton *button = buttons.at(i);
		const int value = choices.at(i).first;
		QObject::connect(button, &QPushButton::clicked, holder,
			[buttons, button, value, chosen] {
				for (QPushButton *other : buttons)
					other->setChecked(other == button);
				chosen(value);
			});
	}
	return holder;
}

} // namespace

FiltersTab::FiltersTab(QWidget *parent)
	: QWidget(parent)
	, m_filters(Filters::load())
{
	QWidget *inner = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(inner);
	layout->setSpacing(6);

	layout->addWidget(sectionLabel(tr("Galleries"), inner));
	layout->addWidget(galleryPills());
	layout->addWidget(hintLabel(
		tr("Any combination. Each chosen gallery is asked once and the pictures "
		   "are taken from them in turn, so none of them decides the whole run: "
		   "three from each of two, two from each of three."),
		inner));

	layout->addWidget(sectionLabel(tr("What to search"), inner));
	layout->addWidget(pillRow(inner,
		{
			{ Filters::Safe, tr("Safe") },
			{ Filters::Adult, tr("NSFW") },
			{ Filters::Everything, tr("All") },
		},
		m_filters.mode,
		[this](int value) {
			m_filters.mode = Filters::Mode(value);
			collect();
		},
		3));
	layout->addWidget(hintLabel(
		tr("Each gallery marks its own pictures, and the mark is all this chooses "
		   "by. The tags below hold in every mode."),
		inner));

	layout->addWidget(sectionLabel(tr("Wallpaper size"), inner));
	layout->addWidget(pillRow(inner,
		{
			{ Filters::Auto, tr("Auto") },
			{ Filters::HD, QStringLiteral("HD") },
			{ Filters::FullHD, QStringLiteral("Full HD") },
			{ Filters::WQHD, QStringLiteral("2K") },
			{ Filters::UHD, QStringLiteral("4K") },
			{ Filters::Custom, tr("Own size") },
			{ Filters::AsItComes, tr("Any") },
		},
		m_filters.size,
		[this](int value) {
			m_filters.size = Filters::Size(value);
			m_customWidth->setEnabled(m_filters.size == Filters::Custom);
			m_customHeight->setEnabled(m_filters.size == Filters::Custom);
			collect();
		},
		4));

	m_customWidth = new QSpinBox(inner);
	m_customWidth->setRange(320, 15360);
	m_customWidth->setSingleStep(10);
	m_customWidth->setValue(m_filters.custom.width());
	m_customHeight = new QSpinBox(inner);
	m_customHeight->setRange(240, 8640);
	m_customHeight->setSingleStep(10);
	m_customHeight->setValue(m_filters.custom.height());
	m_customWidth->setEnabled(m_filters.size == Filters::Custom);
	m_customHeight->setEnabled(m_filters.size == Filters::Custom);
	connect(m_customWidth, &QSpinBox::valueChanged, this, [this] { collect(); });
	connect(m_customHeight, &QSpinBox::valueChanged, this, [this] { collect(); });

	QHBoxLayout *custom = new QHBoxLayout;
	custom->addWidget(m_customWidth);
	custom->addWidget(new QLabel(QStringLiteral("x"), inner));
	custom->addWidget(m_customHeight);
	custom->addStretch();
	layout->addLayout(custom);
	layout->addWidget(hintLabel(
		tr("Auto is what the kernel reports for the largest connected screen. Any "
		   "hands the picture over untouched, for a desktop that would rather "
		   "scale it itself."),
		inner));

	layout->addWidget(sectionLabel(tr("Show pictures tagged"), inner));
	layout->addWidget(wantedGrid());
	layout->addWidget(hintLabel(
		tr("Any pressed tag is enough, and nothing pressed means no preference. A "
		   "tag only one gallery knows is dimmed while the other is the only one "
		   "chosen."),
		inner));

	layout->addWidget(sectionLabel(tr("Never show pictures tagged"), inner));
	layout->addWidget(blockedGrid());
	layout->addWidget(hintLabel(
		tr("Both galleries call these safe. A wallpaper is on show to everyone who "
		   "walks past the machine, so they start pressed."),
		inner));

	m_summary = new QLabel(inner);
	m_summary->setObjectName(QStringLiteral("status"));
	m_summary->setWordWrap(true);
	layout->addSpacing(6);
	layout->addWidget(m_summary);
	layout->addStretch();

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidget(inner);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);

	QVBoxLayout *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(scroll);

	updateTagAvailability();
	collect();
}

QWidget *FiltersTab::galleryPills()
{
	QWidget *holder = new QWidget(this);
	QGridLayout *grid = new QGridLayout(holder);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(6);

	int index = 0;
	for (Provider provider : Filters::allGalleries()) {
		const QString key = Filters::galleryKey(provider);
		QPushButton *button = pill(key, holder);
		button->setChecked(m_filters.galleries.contains(key));
		connect(button, &QPushButton::clicked, this, [this, button, key] {
			// The last one cannot be turned off: a window with no gallery
			// to ask has nothing to show.
			int on = 0;
			for (QPushButton *other : std::as_const(m_galleries))
				on += other->isChecked() ? 1 : 0;
			if (on == 0)
				button->setChecked(true);
			updateTagAvailability();
			collect();
		});
		grid->addWidget(button, 0, index++);
		m_galleries.insert(key, button);
	}
	return holder;
}

QWidget *FiltersTab::wantedGrid()
{
	QWidget *holder = new QWidget(this);
	QGridLayout *grid = new QGridLayout(holder);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(6);

	int index = 0;
	for (const TagChoice &choice : Filters::tagTable()) {
		QPushButton *button = pill(choice.label, holder);
		button->setChecked(m_filters.wanted.contains(choice.label));
		QStringList known;
		if (!choice.nekos.isEmpty())
			known << QStringLiteral("nekos.moe");
		if (!choice.booru.isEmpty())
			known << QStringLiteral("safebooru, danbooru");
		button->setToolTip(tr("Known to %1").arg(known.join(QStringLiteral(" + "))));
		connect(button, &QPushButton::clicked, this, [this] { collect(); });
		grid->addWidget(button, index / kTagColumns, index % kTagColumns);
		m_wanted.insert(choice.label, button);
		++index;
	}
	return holder;
}

QWidget *FiltersTab::blockedGrid()
{
	QWidget *holder = new QWidget(this);
	QGridLayout *grid = new QGridLayout(holder);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(6);

	const QStringList tags = Filters::defaultBlockedTags();
	for (int i = 0; i < tags.size(); ++i) {
		QPushButton *button = pill(tags.at(i), holder, "block");
		button->setChecked(m_filters.blocked.contains(tags.at(i)));
		connect(button, &QPushButton::clicked, this, [this] { collect(); });
		grid->addWidget(button, i / kTagColumns, i % kTagColumns);
		m_blocked.insert(tags.at(i), button);
	}
	return holder;
}

void FiltersTab::updateTagAvailability()
{
	const QVector<Provider> providers = m_filters.providers();
	for (const TagChoice &choice : Filters::tagTable()) {
		QPushButton *button = m_wanted.value(choice.label);
		if (!button)
			continue;
		bool known = false;
		for (Provider provider : providers)
			known = known || choice.knownTo(provider);
		button->setEnabled(known);
		// A tag the chosen gallery cannot answer is not a filter, it is a
		// silence. Let go of it rather than search with it.
		if (!known && button->isChecked())
			button->setChecked(false);
	}
}

void FiltersTab::collect()
{
	m_filters.galleries.clear();
	for (Provider provider : Filters::allGalleries()) {
		const QString key = Filters::galleryKey(provider);
		QPushButton *button = m_galleries.value(key);
		if (button && button->isChecked())
			m_filters.galleries << key;
	}

	m_filters.wanted.clear();
	for (const TagChoice &choice : Filters::tagTable()) {
		QPushButton *button = m_wanted.value(choice.label);
		if (button && button->isEnabled() && button->isChecked())
			m_filters.wanted << choice.label;
	}
	m_filters.blocked.clear();
	for (const QString &tag : Filters::defaultBlockedTags()) {
		QPushButton *button = m_blocked.value(tag);
		if (button && button->isChecked())
			m_filters.blocked << tag;
	}
	if (m_customWidth && m_customHeight)
		m_filters.custom = QSize(m_customWidth->value(), m_customHeight->value());

	m_filters.save();

	if (m_summary) {
		const QString mode = m_filters.mode == Filters::Safe ? tr("safe only")
			: m_filters.mode == Filters::Adult            ? tr("adult only")
								      : tr("everything");
		m_summary->setText(
			tr("%1, %2, %3, %4 tags refused.")
				.arg(m_filters.galleries.join(QStringLiteral(" + ")), mode,
					m_filters.wanted.isEmpty()
						? tr("any subject")
						: m_filters.wanted.join(QStringLiteral(", ")))
				.arg(m_filters.blocked.size()));
	}
	emit changed(m_filters);
}

AutostartTab::AutostartTab(QWidget *parent)
	: QWidget(parent)
	, m_settings(autostart::Settings::load())
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setSpacing(6);

	layout->addWidget(sectionLabel(tr("At login"), this));
	layout->addWidget(pillRow(this,
		{
			{ int(autostart::Login::Never), tr("Never") },
			{ int(autostart::Login::FirstTime), tr("Only the first time") },
			{ int(autostart::Login::EveryLogin), tr("Every login") },
		},
		int(m_settings.login),
		[this](int value) {
			m_settings.login = autostart::Login(value);
			m_settings.save();
			refresh();
		},
		3));
	layout->addWidget(hintLabel(
		tr("The user unit runs at every login and asks this. Only the first time "
		   "leaves a machine that already has a wallpaper alone -- it is how a "
		   "fresh install stops being grey and nothing more."),
		this));

	layout->addWidget(sectionLabel(tr("While the session runs"), this));
	QVector<QPair<int, QString>> rotation;
	for (int minutes : autostart::intervals())
		rotation << qMakePair(minutes, autostart::intervalName(minutes));
	QWidget *timer = pillRow(this, rotation, m_settings.changeEvery,
		[this](int minutes) {
			QString error;
			if (!autostart::applyRotation(minutes, &error)) {
				m_status->setText(error);
				return;
			}
			m_settings.changeEvery = minutes;
			m_settings.save();
			refresh();
		},
		5);
	// Nothing to press where there is nothing to press it on: a build
	// directory has no units and an ssh session has no user manager.
	timer->setEnabled(autostart::available());
	layout->addWidget(timer);
	layout->addWidget(hintLabel(
		tr("A systemd user timer, switched on the moment one is pressed. It runs "
		   "with the desktop session and stops with it, and every picture it "
		   "takes follows the filters as they are then."),
		this));

	m_status = new QLabel(this);
	m_status->setObjectName(QStringLiteral("status"));
	m_status->setWordWrap(true);
	layout->addSpacing(6);
	layout->addWidget(m_status);
	layout->addStretch();

	refresh();
}

void AutostartTab::refresh()
{
	m_status->setText(autostart::describe(m_settings));
}

Window::Window(QWidget *parent)
	: QWidget(parent)
	, m_screen(wallpaper::screenSize())
	, m_target(m_screen)
	, m_picker(new Picker(m_screen, this))
{
	setWindowTitle(tr("nekowall"));
	resize(980, 760);

	m_preview = new QLabel(this);
	m_preview->setObjectName(QStringLiteral("preview"));
	m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setMinimumSize(520, 300);

	m_credit = new QLabel(this);
	m_credit->setObjectName(QStringLiteral("credit"));
	m_credit->setTextFormat(Qt::RichText);
	m_credit->setOpenExternalLinks(true);
	m_credit->setWordWrap(true);

	m_status = new QLabel(this);
	m_status->setObjectName(QStringLiteral("status"));
	m_status->setWordWrap(true);

	m_another = new QPushButton(tr("Another one"), this);
	m_apply = new QPushButton(tr("Use as wallpaper"), this);
	m_apply->setObjectName(QStringLiteral("primary"));
	m_apply->setDefault(true);
	QPushButton *close = new QPushButton(tr("Close"), this);
	for (QPushButton *button : { m_another, m_apply, close })
		button->setCursor(Qt::PointingHandCursor);

	connect(m_another, &QPushButton::clicked, this, &Window::shuffle);
	connect(m_apply, &QPushButton::clicked, this, &Window::applyCurrent);
	connect(close, &QPushButton::clicked, this, &Window::close);

	connect(m_picker, &Picker::progress, this,
		[this](const QString &what) { m_status->setText(what); });
	connect(m_picker, &Picker::picked, this, &Window::showCandidate);
	connect(m_picker, &Picker::failed, this, [this](const QString &reason) {
		setBusy(false);
		m_status->setText(reason);
	});

	QHBoxLayout *buttons = new QHBoxLayout;
	buttons->addWidget(m_another);
	buttons->addStretch();
	buttons->addWidget(close);
	buttons->addWidget(m_apply);

	QWidget *picture = new QWidget(this);
	QVBoxLayout *pictureLayout = new QVBoxLayout(picture);
	pictureLayout->setSpacing(10);
	pictureLayout->addWidget(m_preview, 1);
	pictureLayout->addWidget(m_credit);
	pictureLayout->addWidget(m_status);
	pictureLayout->addLayout(buttons);

	m_filters = new FiltersTab(this);
	// A filter changed is a filter that applies to the next picture, not to
	// the next launch.
	connect(m_filters, &FiltersTab::changed, this, [this](const Filters &filters) {
		m_picker->setFilters(filters);
		m_target = filters.target(m_screen);
		m_picker->setTarget(m_target);
		// A size chosen is a size to see straight away: the picture is
		// already here, only the canvas around it changes.
		render();
	});
	m_picker->setFilters(m_filters->filters());
	m_target = m_filters->filters().target(m_screen);
	m_picker->setTarget(m_target);

	QTabWidget *tabs = new QTabWidget(this);
	tabs->addTab(picture, tr("Wallpaper"));
	tabs->addTab(m_filters, tr("Filters"));
	tabs->addTab(new AutostartTab(this), tr("Autostart"));

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(14, 10, 14, 14);
	layout->addWidget(tabs);

	shuffle();
}

void Window::setBusy(bool busy)
{
	m_another->setEnabled(!busy);
	m_apply->setEnabled(!busy && !m_canvas.isNull());
}

void Window::shuffle()
{
	setBusy(true);
	m_status->setText(tr("Looking for a picture..."));
	m_picker->start();
}

void Window::showCandidate(const QImage &art, const Artwork &meta)
{
	m_art = art;
	m_meta = meta;
	render();

	const QString artist = meta.artist.isEmpty() ? tr("an artist who is not named")
						     : meta.artist.toHtmlEscaped();
	m_credit->setText(tr("Drawn by <b>%1</b> &mdash; <a href=\"%2\">%3</a>")
				  .arg(artist, meta.pageUrl, meta.galleryName()));
	setBusy(false);
}

void Window::render()
{
	if (m_art.isNull())
		return;

	if (!m_target.isValid()) {
		m_canvas = m_art;
		m_cropped = true;
		m_status->setText(tr("%1x%2, the picture as the artist left it.")
					  .arg(m_art.width())
					  .arg(m_art.height()));
	} else {
		m_canvas = wallpaper::compose(m_art, m_target, &m_cropped);
		m_status->setText(m_cropped ? tr("%1x%2, the picture covers it.")
							.arg(m_target.width())
							.arg(m_target.height())
					    : tr("%1x%2, the picture is whole on a blurred "
						 "backdrop of itself.")
							.arg(m_target.width())
							.arg(m_target.height()));
	}
	updatePreview();
	setBusy(false);
}

void Window::updatePreview()
{
	if (m_canvas.isNull())
		return;
	m_preview->setPixmap(QPixmap::fromImage(m_canvas).scaled(
		m_preview->size() - QSize(12, 12), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Window::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updatePreview();
}

void Window::applyCurrent()
{
	if (m_canvas.isNull())
		return;

	QString error;
	const QString path = wallpaper::store(m_canvas, m_meta, m_cropped, &error);
	if (path.isEmpty()) {
		m_status->setText(error);
		return;
	}
	if (!wallpaper::apply(path, &error)) {
		m_status->setText(error);
		return;
	}
	m_status->setText(tr("Done. The wallpaper is yours until the next one."));
}
