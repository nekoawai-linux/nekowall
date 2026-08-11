#include "filters.h"
#include "source.h"
#include "wallpaper.h"
#include "window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTextStream>
#include <QTimer>

namespace {

void usage(QTextStream &out)
{
	out << "Usage: nekowall [--set|--current|--version|--help]\n\n"
	    << "Random art from nekos.moe as the desktop wallpaper, drawn to the\n"
	    << "resolution the machine is running.\n\n"
	    << "  (no argument)  open the window and pick a picture by hand\n"
	    << "  --set          take one and apply it, without a window\n"
	    << "  --current      who drew the wallpaper that is set now\n"
	    << "  --version\n"
	    << "  --help\n\n"
	    << "The window has two tabs: the picture, and filters -- what to\n"
	    << "search (safe, nsfw, all), the size of the wallpaper, and the\n"
	    << "tags to want or never to see, as boxes to tick.\n\n"
	    << "Those choices are kept in ~/.config/nekowall/nekowall.ini and\n"
	    << "are what --set follows.\n";
}

// The headless path: one picture, applied, then out. This is what the user
// service runs at the first login.
int setOnce(int argc, char *argv[])
{
	QGuiApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("nekowall"));

	QTextStream out(stdout);
	QTextStream err(stderr);
	int status = 0;

	const Filters filters = Filters::load();
	const QSize screen = wallpaper::screenSize();
	// An invalid target is the "any size" setting: the picture is applied
	// as it was downloaded.
	const QSize target = filters.target(screen);
	Picker picker(target);
	picker.setFilters(filters);

	QObject::connect(&picker, &Picker::picked, &app,
		[&](const QImage &art, const Artwork &meta) {
			bool cropped = true;
			const QImage canvas = target.isValid()
				? wallpaper::compose(art, target, &cropped)
				: art;

			QString error;
			const QString path = wallpaper::store(canvas, meta, cropped, &error);
			if (path.isEmpty()) {
				err << error << '\n';
				status = 1;
			} else if (!wallpaper::apply(path, &error)) {
				err << error << '\n';
				status = 1;
			} else {
				out << "wallpaper: " << path << '\n'
				    << "artist:    "
				    << (meta.artist.isEmpty() ? QStringLiteral("unknown")
							      : meta.artist)
				    << '\n'
				    << "picture:   " << meta.pageUrl() << '\n';
			}
			app.quit();
		});

	QObject::connect(&picker, &Picker::failed, &app, [&](const QString &reason) {
		err << reason << '\n';
		status = 1;
		app.quit();
	});

	QTimer::singleShot(0, &picker, &Picker::start);
	app.exec();
	return status;
}

} // namespace

int main(int argc, char *argv[])
{
	QTextStream out(stdout);
	QString mode;
	for (int i = 1; i < argc; ++i)
		mode = QString::fromLocal8Bit(argv[i]);

	if (mode == QLatin1String("--help") || mode == QLatin1String("-h")) {
		usage(out);
		return 0;
	}
	if (mode == QLatin1String("--version") || mode == QLatin1String("-V")) {
		out << "nekowall " << NEKOWALL_VERSION << '\n';
		return 0;
	}
	if (mode == QLatin1String("--current")) {
		// No display needed to read a file, and none is asked for: this
		// answers over ssh as well as it does on the desktop.
		QCoreApplication app(argc, argv);
		out << wallpaper::describeCurrent() << '\n';
		return 0;
	}
	if (mode == QLatin1String("--set"))
		return setOnce(argc, argv);
	if (!mode.isEmpty()) {
		QTextStream(stderr) << "nekowall: unknown argument: " << mode << '\n';
		usage(out);
		return 2;
	}

	QApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("nekowall"));
	Window window;
	window.show();
	return app.exec();
}
