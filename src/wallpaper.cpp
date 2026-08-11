#include "wallpaper.h"

#include "source.h"

#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

#include <csignal>

namespace {

// Blur by throwing pixels away and asking for them back: a twentieth of the
// size and up again is indistinguishable from a real blur once it is behind
// the picture, and it costs one scale instead of a convolution.
QImage blurred(const QImage &source)
{
	const QSize small = source.size() / 20;
	if (small.width() < 2 || small.height() < 2)
		return source;
	return source.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
		.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

bool run(const QString &program, const QStringList &arguments, QString *output = nullptr)
{
	QProcess process;
	process.start(program, arguments);
	if (!process.waitForFinished(15000))
		return false;
	if (output)
		*output = QString::fromUtf8(process.readAllStandardOutput());
	return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool applyXfce(const QString &path, QString *error)
{
	QString properties;
	if (!run(QStringLiteral("xfconf-query"),
		    { QStringLiteral("-c"), QStringLiteral("xfce4-desktop"), QStringLiteral("-l") },
		    &properties)) {
		*error = QObject::tr("xfconf-query is not available.");
		return false;
	}

	int changed = 0;
	const QStringList lines = properties.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
	for (const QString &line : lines) {
		const QString property = line.trimmed();
		if (!property.endsWith(QLatin1String("/last-image")))
			continue;
		if (run(QStringLiteral("xfconf-query"),
			    { QStringLiteral("-c"), QStringLiteral("xfce4-desktop"),
				    QStringLiteral("-p"), property, QStringLiteral("-s"), path }))
			++changed;
		// Style 5 is "zoomed"; the canvas already has the screen's shape, so
		// the choice only matters when the screen changes underneath it.
		QString style = property;
		style.replace(QLatin1String("/last-image"), QLatin1String("/image-style"));
		run(QStringLiteral("xfconf-query"),
			{ QStringLiteral("-c"), QStringLiteral("xfce4-desktop"),
				QStringLiteral("-p"), style, QStringLiteral("-s"),
				QStringLiteral("5") });
	}

	if (changed > 0)
		return true;

	// A session that has never had a wallpaper set has no such property yet,
	// so create the one xfdesktop looks at first.
	if (run(QStringLiteral("xfconf-query"),
		    { QStringLiteral("-c"), QStringLiteral("xfce4-desktop"), QStringLiteral("-p"),
			    QStringLiteral("/backdrop/screen0/monitor0/workspace0/last-image"),
			    QStringLiteral("-n"), QStringLiteral("-t"), QStringLiteral("string"),
			    QStringLiteral("-s"), path }))
		return true;

	*error = QObject::tr("Xfce did not accept the wallpaper.");
	return false;
}

bool applyGnome(const QString &path, QString *error)
{
	const QString uri = QStringLiteral("file://") + path;
	const bool light = run(QStringLiteral("gsettings"),
		{ QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"),
			QStringLiteral("picture-uri"), uri });
	// Since GNOME 42 the dark theme has a wallpaper of its own, and setting
	// only the light one leaves half the sessions unchanged.
	run(QStringLiteral("gsettings"),
		{ QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"),
			QStringLiteral("picture-uri-dark"), uri });
	run(QStringLiteral("gsettings"),
		{ QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"),
			QStringLiteral("picture-options"), QStringLiteral("zoom") });
	if (!light)
		*error = QObject::tr("gsettings did not accept the wallpaper.");
	return light;
}

bool applyPlasma(const QString &path, QString *error)
{
	if (run(QStringLiteral("plasma-apply-wallpaperimage"), { path }))
		return true;
	*error = QObject::tr("plasma-apply-wallpaperimage failed.");
	return false;
}

bool applySwaybg(const QString &path, QString *error)
{
	// swaybg draws until it is killed, so the previous one goes first.
	run(QStringLiteral("pkill"), { QStringLiteral("-x"), QStringLiteral("swaybg") });

	qint64 pid = 0;
	if (!QProcess::startDetached(QStringLiteral("swaybg"),
		    { QStringLiteral("-m"), QStringLiteral("fill"), QStringLiteral("-i"), path },
		    QString(), &pid)) {
		*error = QObject::tr("swaybg is not installed.");
		return false;
	}

	// Starting is not drawing: swaybg without a compositor to talk to exits
	// a moment later, and reporting success then is how a wallpaper that was
	// never set gets called done.
	QThread::msleep(400);
	if (::kill(static_cast<pid_t>(pid), 0) != 0) {
		*error = QObject::tr("swaybg started and stopped at once. Is this session Wayland?");
		return false;
	}
	return true;
}

bool applyHyprland(const QString &path, QString *error)
{
	if (run(QStringLiteral("hyprctl"),
		    { QStringLiteral("hyprpaper"), QStringLiteral("preload"), path })
		&& run(QStringLiteral("hyprctl"),
			{ QStringLiteral("hyprpaper"), QStringLiteral("wallpaper"),
				QStringLiteral(",") + path }))
		return true;
	// hyprpaper is a recommendation, not a requirement; without it the
	// generic Wayland path still draws something.
	return applySwaybg(path, error);
}

} // namespace

namespace wallpaper {

QString desktopName()
{
	const QString session = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
	if (!session.isEmpty())
		return session;
	if (!qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE").isEmpty())
		return QStringLiteral("hyprland");
	if (!qEnvironmentVariable("NIRI_SOCKET").isEmpty())
		return QStringLiteral("niri");
	return QString();
}

QSize screenSize()
{
	// /sys/class/drm holds the modes of every connector the kernel drives.
	// The first mode of a connected one is the mode it is running.
	QSize best;
	const QDir drm(QStringLiteral("/sys/class/drm"));
	const QStringList connectors = drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QString &connector : connectors) {
		QFile status(drm.filePath(connector + QStringLiteral("/status")));
		if (!status.open(QIODevice::ReadOnly | QIODevice::Text))
			continue;
		if (QString::fromLatin1(status.readAll()).trimmed() != QLatin1String("connected"))
			continue;

		QFile modes(drm.filePath(connector + QStringLiteral("/modes")));
		if (!modes.open(QIODevice::ReadOnly | QIODevice::Text))
			continue;
		const QString first = QString::fromLatin1(modes.readLine()).trimmed();
		const QStringList parts = first.split(QLatin1Char('x'));
		if (parts.size() != 2)
			continue;
		const QSize mode(parts.at(0).toInt(), parts.at(1).toInt());
		if (mode.width() <= 0 || mode.height() <= 0)
			continue;
		// One canvas is drawn for every screen, so it is made for the
		// largest of them and the smaller ones scale it down.
		if (qint64(mode.width()) * mode.height() > qint64(best.width()) * best.height())
			best = mode;
	}
	if (best.isValid())
		return best;

	if (QGuiApplication::instance()) {
		if (const QScreen *screen = QGuiApplication::primaryScreen()) {
			const QSize pixels = screen->geometry().size() * screen->devicePixelRatio();
			if (pixels.width() > 0 && pixels.height() > 0)
				return pixels;
		}
	}
	return QSize(1920, 1080);
}

QImage compose(const QImage &art, QSize screen, bool *cropped)
{
	QImage canvas(screen, QImage::Format_RGB32);
	canvas.fill(Qt::black);

	QPainter painter(&canvas);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

	const bool fits = Picker::cropLoss(art.size(), screen) <= 0.25;
	if (cropped)
		*cropped = fits;

	if (fits) {
		const QImage filled = art.scaled(screen, Qt::KeepAspectRatioByExpanding,
			Qt::SmoothTransformation);
		painter.drawImage(QPoint((screen.width() - filled.width()) / 2,
					  (screen.height() - filled.height()) / 2),
			filled);
		return canvas;
	}

	const QImage backdrop = blurred(art.scaled(screen, Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation));
	painter.drawImage(QPoint((screen.width() - backdrop.width()) / 2,
				  (screen.height() - backdrop.height()) / 2),
		backdrop);
	// Desktop icons and panels sit on this backdrop; darkening it keeps
	// their labels readable whatever the picture happens to be.
	painter.fillRect(canvas.rect(), QColor(0, 0, 0, 110));

	const QImage front = art.scaled(screen, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	painter.drawImage(QPoint((screen.width() - front.width()) / 2,
				  (screen.height() - front.height()) / 2),
		front);
	return canvas;
}

QString stateDirectory()
{
	return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
		+ QStringLiteral("/nekowall");
}

QString store(const QImage &canvas, const Artwork &art, bool cropped, QString *error)
{
	const QString directory = stateDirectory();
	if (!QDir().mkpath(directory)) {
		*error = QObject::tr("Cannot create %1").arg(directory);
		return QString();
	}

	const QString path = directory + QStringLiteral("/wallpaper-") + art.id
		+ QStringLiteral(".jpg");
	if (!canvas.save(path, "JPEG", 92)) {
		*error = QObject::tr("Cannot write %1").arg(path);
		return QString();
	}

	QJsonObject state;
	state[QStringLiteral("id")] = art.id;
	state[QStringLiteral("artist")] = art.artist.isEmpty() ? QStringLiteral("unknown")
							       : art.artist;
	state[QStringLiteral("page")] = art.pageUrl;
	state[QStringLiteral("gallery")] = art.galleryName();
	state[QStringLiteral("file")] = path;
	state[QStringLiteral("screen")] = QStringLiteral("%1x%2")
						 .arg(canvas.width())
						 .arg(canvas.height());
	state[QStringLiteral("fit")] = cropped ? QStringLiteral("filled")
					       : QStringLiteral("on a blurred backdrop");
	state[QStringLiteral("appliedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);

	QFile file(directory + QStringLiteral("/current.json"));
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		file.write(QJsonDocument(state).toJson(QJsonDocument::Indented));

	// One wallpaper is kept, the one on screen. Older files are somebody
	// else's disk space.
	const QDir directoryHandle(directory);
	const QStringList old = directoryHandle.entryList({ QStringLiteral("wallpaper-*.jpg") },
		QDir::Files);
	for (const QString &name : old) {
		if (directoryHandle.filePath(name) != path)
			QFile::remove(directoryHandle.filePath(name));
	}
	return path;
}

bool apply(const QString &path, QString *error)
{
	QString ignored;
	if (!error)
		error = &ignored;
	*error = QString();

	const QString desktop = desktopName();
	if (desktop.contains(QLatin1String("xfce")))
		return applyXfce(path, error);
	if (desktop.contains(QLatin1String("gnome")))
		return applyGnome(path, error);
	if (desktop.contains(QLatin1String("kde")) || desktop.contains(QLatin1String("plasma")))
		return applyPlasma(path, error);
	if (desktop.contains(QLatin1String("hyprland")))
		return applyHyprland(path, error);
	if (desktop.contains(QLatin1String("niri")) || desktop.contains(QLatin1String("sway"))
		|| desktop.contains(QLatin1String("wlroots")))
		return applySwaybg(path, error);

	if (desktop.isEmpty()) {
		*error = QObject::tr("No desktop session found. The wallpaper is at %1").arg(path);
		return false;
	}
	// An unknown Wayland desktop still has a fair chance with swaybg, and
	// saying which desktop was not recognised is worth more than a shrug.
	if (applySwaybg(path, error))
		return true;
	*error = QObject::tr("Unknown desktop: %1. The wallpaper is at %2")
			 .arg(desktop, path);
	return false;
}

QString describeCurrent()
{
	QFile file(stateDirectory() + QStringLiteral("/current.json"));
	if (!file.open(QIODevice::ReadOnly))
		return QObject::tr("No wallpaper has been set by nekowall yet.");

	const QJsonObject state = QJsonDocument::fromJson(file.readAll()).object();
	return QObject::tr("Artist:  %1\nPicture: %2\nFile:    %3\nScreen:  %4, %5\nSet:     %6")
		.arg(state.value(QStringLiteral("artist")).toString(),
			state.value(QStringLiteral("page")).toString(),
			state.value(QStringLiteral("file")).toString(),
			state.value(QStringLiteral("screen")).toString(),
			state.value(QStringLiteral("fit")).toString(),
			state.value(QStringLiteral("appliedAt")).toString());
}

} // namespace wallpaper
