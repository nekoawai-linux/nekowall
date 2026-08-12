#pragma once

#include <QString>
#include <QVector>

// Whether nekowall does anything on its own. Two questions, and they are not
// the same one: what happens at login, and what happens while the session
// goes on.
//
// The first is a setting and nothing more -- the login unit is always there
// and asks this file what it is for. The second needs a systemd user timer,
// because nothing else can wake a program up in six hours.
namespace autostart {

enum class Login {
	Never, // the unit runs and leaves; nekowall stays a window and a command
	FirstTime, // only on a system that has no wallpaper of its own yet
	EveryLogin, // a different picture at every login
};

struct Settings {
	Login login = Login::FirstTime;
	// Minutes between pictures while the session runs. Zero is off.
	int changeEvery = 0;

	static Settings load();
	void save() const;
};

QString loginKey(Login login);
Login loginFromKey(const QString &key);

// The intervals the window offers, in minutes. Written down rather than
// typed: a timer that fires every three minutes is a way to be shown three
// hundred pictures nobody asked for.
QVector<int> intervals();
QString intervalName(int minutes);

// True when there is a systemd user manager to talk to and the units are
// where the package puts them. A nekowall that was built rather than
// installed has neither, and saying so is worth more than a failed
// systemctl.
bool available();
QString unavailableReason();

// Enables or disables the timer and writes the interval it should keep.
// Nothing here touches the login setting: that unit is enabled once, for
// everyone, when the package is installed.
bool applyRotation(int minutes, QString *error);

// What the machine will actually do, in one line. Read from systemd where
// systemd is the one who knows.
QString describe(const Settings &settings);

// Answers `--at-login`: whether this run should set a wallpaper at all.
bool wantedAtLogin(const Settings &settings);

} // namespace autostart
