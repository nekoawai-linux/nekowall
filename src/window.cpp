#include "window.h"

#include "wallpaper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

Window::Window(QWidget *parent)
	: QWidget(parent)
	, m_screen(wallpaper::screenSize())
	, m_picker(new Picker(m_screen, this))
{
	setWindowTitle(tr("nekowall"));
	resize(900, 640);

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

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(m_preview, 1);
	layout->addWidget(m_credit);
	layout->addWidget(m_status);
	layout->addLayout(buttons);

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
	m_canvas = wallpaper::compose(art, m_screen, &m_cropped);
	m_meta = meta;
	updatePreview();

	const QString artist = meta.artist.isEmpty() ? tr("unknown artist") : meta.artist;
	m_credit->setText(tr("Drawn by <b>%1</b> &mdash; <a href=\"%2\">%2</a>")
				  .arg(artist.toHtmlEscaped(), meta.pageUrl()));
	m_status->setText(m_cropped
			? tr("%1x%2, the picture covers the screen.").arg(m_screen.width()).arg(m_screen.height())
			: tr("%1x%2, the picture is whole on a blurred backdrop of itself.")
				  .arg(m_screen.width())
				  .arg(m_screen.height()));
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
