#include "window.h"

#include "wallpaper.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

const int kTagColumns = 4;

} // namespace

FiltersTab::FiltersTab(QWidget *parent)
	: QWidget(parent)
	, m_filters(Filters::load())
{
	QRadioButton *safe = new QRadioButton(tr("Safe"), this);
	QRadioButton *adult = new QRadioButton(tr("NSFW"), this);
	QRadioButton *all = new QRadioButton(tr("All"), this);
	safe->setToolTip(tr("Only pictures the gallery marks as safe."));
	adult->setToolTip(tr("Only pictures the gallery marks as adult."));
	all->setToolTip(tr("Both, whatever comes."));

	switch (m_filters.mode) {
	case Filters::Adult:
		adult->setChecked(true);
		break;
	case Filters::Everything:
		all->setChecked(true);
		break;
	case Filters::Safe:
		safe->setChecked(true);
		break;
	}

	connect(safe, &QRadioButton::toggled, this, [this](bool on) {
		if (on) {
			m_filters.mode = Filters::Safe;
			collect();
		}
	});
	connect(adult, &QRadioButton::toggled, this, [this](bool on) {
		if (on) {
			m_filters.mode = Filters::Adult;
			collect();
		}
	});
	connect(all, &QRadioButton::toggled, this, [this](bool on) {
		if (on) {
			m_filters.mode = Filters::Everything;
			collect();
		}
	});

	QHBoxLayout *modes = new QHBoxLayout;
	modes->addWidget(safe);
	modes->addWidget(adult);
	modes->addWidget(all);
	modes->addStretch();

	QGroupBox *modeBox = new QGroupBox(tr("What to search"), this);
	QVBoxLayout *modeLayout = new QVBoxLayout(modeBox);
	modeLayout->addLayout(modes);
	QLabel *modeHint = new QLabel(
		tr("The gallery marks its own pictures, and the mark is all this "
		   "chooses by. The tags below hold in every mode."),
		modeBox);
	modeHint->setWordWrap(true);
	modeLayout->addWidget(modeHint);

	QGroupBox *sizeBox = new QGroupBox(tr("Wallpaper size"), this);
	QVBoxLayout *sizeLayout = new QVBoxLayout(sizeBox);
	sizeLayout->addWidget(sizeChoices());
	QLabel *sizeHint = new QLabel(
		tr("Auto is what the kernel reports for the largest connected screen. "
		   "Any means the picture is handed over untouched, for a desktop "
		   "that would rather scale it itself."),
		sizeBox);
	sizeHint->setWordWrap(true);
	sizeLayout->addWidget(sizeHint);

	QGroupBox *wantedBox = new QGroupBox(tr("Show only pictures tagged"), this);
	QVBoxLayout *wantedLayout = new QVBoxLayout(wantedBox);
	QLabel *wantedHint = new QLabel(
		tr("Any of the ticked tags is enough. Nothing ticked means no "
		   "preference at all, which is also the fastest."),
		wantedBox);
	wantedHint->setWordWrap(true);
	wantedLayout->addWidget(wantedHint);
	wantedLayout->addWidget(tagBoxes(Filters::offeredTags(), m_filters.wanted, &m_wanted));

	QGroupBox *blockedBox = new QGroupBox(tr("Never show pictures tagged"), this);
	QVBoxLayout *blockedLayout = new QVBoxLayout(blockedBox);
	QLabel *blockedHint = new QLabel(
		tr("The gallery calls these safe. A wallpaper is on show to "
		   "everyone who walks past the machine, so they start ticked."),
		blockedBox);
	blockedHint->setWordWrap(true);
	blockedLayout->addWidget(blockedHint);
	blockedLayout->addWidget(
		tagBoxes(Filters::defaultBlockedTags(), m_filters.blocked, &m_blocked));

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);

	QWidget *inner = new QWidget;
	QVBoxLayout *innerLayout = new QVBoxLayout(inner);
	innerLayout->addWidget(modeBox);
	innerLayout->addWidget(sizeBox);
	innerLayout->addWidget(wantedBox);
	innerLayout->addWidget(blockedBox);
	innerLayout->addWidget(m_summary);
	innerLayout->addStretch();

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidget(inner);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(scroll);

	collect();
}

QWidget *FiltersTab::tagBoxes(const QStringList &tags, const QStringList &ticked,
	QList<QCheckBox *> *into)
{
	QWidget *holder = new QWidget(this);
	QGridLayout *grid = new QGridLayout(holder);
	grid->setContentsMargins(0, 0, 0, 0);

	for (int i = 0; i < tags.size(); ++i) {
		QCheckBox *box = new QCheckBox(tags.at(i), holder);
		box->setChecked(ticked.contains(tags.at(i)));
		connect(box, &QCheckBox::toggled, this, [this] { collect(); });
		grid->addWidget(box, i / kTagColumns, i % kTagColumns);
		into->append(box);
	}
	return holder;
}

QWidget *FiltersTab::sizeChoices()
{
	QWidget *holder = new QWidget(this);
	QGridLayout *grid = new QGridLayout(holder);
	grid->setContentsMargins(0, 0, 0, 0);

	const QVector<QPair<Filters::Size, QString>> choices = {
		{ Filters::Auto, tr("Auto") },
		{ Filters::HD, tr("HD 1280x720") },
		{ Filters::FullHD, tr("Full HD 1920x1080") },
		{ Filters::WQHD, tr("2K 2560x1440") },
		{ Filters::UHD, tr("4K 3840x2160") },
		{ Filters::Custom, tr("Own size") },
		{ Filters::AsItComes, tr("Any, as it comes") },
	};

	m_customWidth = new QSpinBox(holder);
	m_customWidth->setRange(320, 15360);
	m_customWidth->setSingleStep(10);
	m_customWidth->setValue(m_filters.custom.width());
	m_customHeight = new QSpinBox(holder);
	m_customHeight->setRange(240, 8640);
	m_customHeight->setSingleStep(10);
	m_customHeight->setValue(m_filters.custom.height());

	for (int i = 0; i < choices.size(); ++i) {
		QRadioButton *button = new QRadioButton(choices.at(i).second, holder);
		const Filters::Size size = choices.at(i).first;
		button->setChecked(m_filters.size == size);
		connect(button, &QRadioButton::toggled, this, [this, size](bool on) {
			if (!on)
				return;
			m_filters.size = size;
			m_customWidth->setEnabled(size == Filters::Custom);
			m_customHeight->setEnabled(size == Filters::Custom);
			collect();
		});
		grid->addWidget(button, i / 4, i % 4);
	}

	QHBoxLayout *custom = new QHBoxLayout;
	custom->addWidget(m_customWidth);
	custom->addWidget(new QLabel(QStringLiteral("x"), holder));
	custom->addWidget(m_customHeight);
	custom->addStretch();
	grid->addLayout(custom, (choices.size() + 3) / 4, 0, 1, 4);

	m_customWidth->setEnabled(m_filters.size == Filters::Custom);
	m_customHeight->setEnabled(m_filters.size == Filters::Custom);
	connect(m_customWidth, &QSpinBox::valueChanged, this, [this] { collect(); });
	connect(m_customHeight, &QSpinBox::valueChanged, this, [this] { collect(); });
	return holder;
}

void FiltersTab::collect()
{
	m_filters.wanted.clear();
	for (QCheckBox *box : std::as_const(m_wanted)) {
		if (box->isChecked())
			m_filters.wanted << box->text();
	}
	m_filters.blocked.clear();
	for (QCheckBox *box : std::as_const(m_blocked)) {
		if (box->isChecked())
			m_filters.blocked << box->text();
	}
	m_filters.custom = QSize(m_customWidth->value(), m_customHeight->value());

	m_filters.save();

	m_summary->setText(tr("Now: %1, %2, %3 tags never shown.")
				   .arg(m_filters.mode == Filters::Safe ? tr("safe pictures only")
						   : m_filters.mode == Filters::Adult
						   ? tr("adult pictures only")
						   : tr("everything the gallery has"),
					   m_filters.wanted.isEmpty()
						   ? tr("any subject")
						   : tr("%1 tags wanted").arg(m_filters.wanted.size()))
				   .arg(m_filters.blocked.size()));

	emit changed(m_filters);
}

Window::Window(QWidget *parent)
	: QWidget(parent)
	, m_screen(wallpaper::screenSize())
	, m_target(m_screen)
	, m_picker(new Picker(m_screen, this))
{
	setWindowTitle(tr("nekowall"));
	resize(960, 720);

	m_preview = new QLabel(this);
	m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setMinimumSize(480, 270);
	m_preview->setStyleSheet(QStringLiteral("background: #17141c; border-radius: 6px;"));

	m_credit = new QLabel(this);
	m_credit->setTextFormat(Qt::RichText);
	m_credit->setOpenExternalLinks(true);
	m_credit->setWordWrap(true);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	m_another = new QPushButton(tr("Another one"), this);
	m_apply = new QPushButton(tr("Use as wallpaper"), this);
	m_apply->setDefault(true);
	QPushButton *close = new QPushButton(tr("Close"), this);

	connect(m_another, &QPushButton::clicked, this, &Window::shuffle);
	connect(m_apply, &QPushButton::clicked, this, &Window::applyCurrent);
	connect(close, &QPushButton::clicked, this, &Window::close);

	connect(m_picker, &Picker::progress, this, [this](const QString &what) {
		m_status->setText(what);
	});
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

	QVBoxLayout *layout = new QVBoxLayout(this);
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
	m_status->setText(tr("Asking nekos.moe for pictures..."));
	m_picker->start();
}

void Window::showCandidate(const QImage &art, const Artwork &meta)
{
	m_art = art;
	m_meta = meta;
	render();

	const QString artist = meta.artist.isEmpty() ? tr("unknown artist") : meta.artist;
	m_credit->setText(tr("Drawn by <b>%1</b> &mdash; <a href=\"%2\">%2</a>")
				  .arg(artist.toHtmlEscaped(), meta.pageUrl()));
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
		m_status->setText(m_cropped
				? tr("%1x%2, the picture covers it.")
					  .arg(m_target.width())
					  .arg(m_target.height())
				: tr("%1x%2, the picture is whole on a blurred backdrop of "
				     "itself.")
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
	m_preview->setPixmap(QPixmap::fromImage(m_canvas).scaled(m_preview->size(),
		Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
