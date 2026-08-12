#include "autostart.h"

#include "wallpaper.h"

#include <QDir>
#include <QFile>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QVector>

namespace {

const QLatin1String kTimer("nekowall-rotate.timer");

QSettings store()
{
	// The same file the filters live in: one window, one place to look.
	return QSettings(QSettings::IniFormat, QSettings::UserScope,
		QStringLiteral("nekowall"), QStringLiteral("nekowall"));
}

bool systemctl(const QStringList &arguments, QString *output = nullptr,
	QString *complaint = nullptr)
{
	QProcess process;
	process.start(QStringLiteral("systemctl"),
		QStringList { QStringLiteral("--user") } + arguments);
	if (!process.waitForFinished(10000)) {
		process.kill();
		if (complaint)
			*complaint = QObject::tr("systemctl did not answer.");
		return false;
	}
	if (output)
		*output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
	if (complaint)
		*complaint = QString::fromUtf8(process.readAllStandardError()).trimmed();
	return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

// Where a user unit may come from. The package writes into the last of them;
// the first is where a drop-in of ours goes.
QStringList unitDirectories()
{
	return {
		QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
			+ QStringLiteral("/systemd/user"),
		QStringLiteral("/etc/systemd/user"),
		QStringLiteral("/usr/local/lib/systemd/user"),
		QStringLiteral("/usr/lib/systemd/user"),
		QStringLiteral("/lib/systemd/user"),
	};
}

bool unitInstalled()
{
	for (const QString &directory : unitDirectories()) {
		if (QFile::exists(directory + QLatin1Char('/') + kTimer))
			return true;
	}
	return false;
}

// The user manager keeps its socket here for as long as it runs, so this is
// the cheapest honest answer to "is there anything listening".
bool managerRunning()
{
	const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
	return !runtime.isEmpty()
		&& QFile::exists(runtime + QStringLiteral("/systemd/private"));
}

QString dropInDirectory()
{
	return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
		+ QStringLiteral("/systemd/user/") + kTimer + QStringLiteral(".d");
}

bool writeInterval(int minutes, QString *error)
{
	const QString directory = dropInDirectory();
	if (!QDir().mkpath(directory)) {
		*error = QObject::tr("Cannot create %1").arg(directory);
		return false;
	}

	QFile file(directory + QStringLiteral("/interval.conf"));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		*error = QObject::tr("Cannot write %1").arg(file.fileName());
		return false;
	}

	// Timer settings are lists: a second OnUnitActiveSec adds a trigger next
	// to the one the package shipped instead of replacing it. The empty
	// assignment is how a drop-in clears the list before writing its own.
	QTextStream out(&file);
	out << "# Written by nekowall. The interval chosen in the window.\n"
	    << "[Timer]\n"
	    << "OnActiveSec=\n"
	    << "OnActiveSec=" << minutes << "min\n"
	    << "OnUnitActiveSec=\n"
	    << "OnUnitActiveSec=" << minutes << "min\n";
	return true;
}

} // namespace

namespace autostart {

QString loginKey(Login login)
{
	switch (login) {
	case Login::Never:
		return QStringLiteral("off");
	case Login::EveryLogin:
		return QStringLiteral("login");
	case Login::FirstTime:
		break;
	}
	return QStringLiteral("once");
}

Login loginFromKey(const QString &key)
{
	if (key == QLatin1String("off"))
		return Login::Never;
	if (key == QLatin1String("login"))
		return Login::EveryLogin;
	return Login::FirstTime;
}

QVector<int> intervals()
{
	return { 0, 60, 180, 360, 1440 };
}

QString intervalName(int minutes)
{
	switch (minutes) {
	case 60:
		return QObject::tr("Every hour");
	case 180:
		return QObject::tr("Every 3 hours");
	case 360:
		return QObject::tr("Every 6 hours");
	case 1440:
		return QObject::tr("Every day");
	default:
		break;
	}
	return QObject::tr("Never");
}

Settings Settings::load()
{
	QSettings settings = store();
	Settings loaded;
	loaded.login = loginFromKey(settings
			.value(QStringLiteral("autostart"), loginKey(Login::FirstTime))
			.toString());
	loaded.changeEvery = settings.value(QStringLiteral("changeEvery"), 0).toInt();
	if (!intervals().contains(loaded.changeEvery))
		loaded.changeEvery = 0;
	return loaded;
}

void Settings::save() const
{
	QSettings settings = store();
	settings.setValue(QStringLiteral("autostart"), loginKey(login));
	settings.setValue(QStringLiteral("changeEvery"), changeEvery);
}

bool available()
{
	return managerRunning() && unitInstalled();
}

QString unavailableReason()
{
	if (!unitInstalled()) {
		return QObject::tr("The systemd units are not installed, so there is nothing "
				   "to wake nekowall up. This build was not installed as a "
				   "package.");
	}
	if (!managerRunning()) {
		return QObject::tr("There is no systemd user manager in this session, so the "
				   "timer cannot be set from here.");
	}
	return QString();
}

bool applyRotation(int minutes, QString *error)
{
	QString ignored;
	if (!error)
		error = &ignored;
	*error = QString();

	if (!available()) {
		*error = unavailableReason();
		return false;
	}
	if (minutes > 0 && !writeInterval(minutes, error))
		return false;

	QString complaint;
	if (!systemctl({ QStringLiteral("daemon-reload") }, nullptr, &complaint)) {
		*error = complaint.isEmpty() ? QObject::tr("systemctl daemon-reload failed.")
					     : complaint;
		return false;
	}

	const QStringList command = minutes > 0
		? QStringList { QStringLiteral("enable"), QStringLiteral("--now"), kTimer }
		: QStringList { QStringLiteral("disable"), QStringLiteral("--now"), kTimer };
	if (!systemctl(command, nullptr, &complaint)) {
		*error = complaint.isEmpty() ? QObject::tr("systemctl %1 failed.").arg(command.first())
					     : complaint;
		return false;
	}
	return true;
}

QString describe(const Settings &settings)
{
	QString atLogin;
	switch (settings.login) {
	case Login::Never:
		atLogin = QObject::tr("Nothing happens at login.");
		break;
	case Login::FirstTime:
		atLogin = QObject::tr("A wallpaper is set at login on a system that has none.");
		break;
	case Login::EveryLogin:
		atLogin = QObject::tr("A new wallpaper at every login.");
		break;
	}

	if (!available())
		return atLogin + QLatin1Char(' ') + unavailableReason();

	// What systemd says, not what the settings file says: the two can drift
	// apart -- a timer enabled by hand, a drop-in edited -- and the one that
	// decides is systemd.
	QString state;
	systemctl({ QStringLiteral("is-active"), kTimer }, &state);
	if (state != QLatin1String("active"))
		return atLogin + QLatin1Char(' ') + QObject::tr("Nothing changes it after that.");
	if (settings.changeEvery <= 0) {
		return atLogin + QLatin1Char(' ')
			+ QObject::tr("A timer is changing it, though this window did not ask "
				      "for one.");
	}
	return atLogin + QLatin1Char(' ')
		+ QObject::tr("Then %1, by a timer of its own.")
			  .arg(intervalName(settings.changeEvery).toLower());
}

bool wantedAtLogin(const Settings &settings)
{
	switch (settings.login) {
	case Login::Never:
		return false;
	case Login::EveryLogin:
		return true;
	case Login::FirstTime:
		break;
	}
	// Once there is a wallpaper, the choice belongs to whoever is sitting
	// there. This is the first login of a system that has none.
	return !QFile::exists(wallpaper::stateDirectory() + QStringLiteral("/current.json"));
}

} // namespace autostart
