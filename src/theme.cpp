#include "theme.h"

QString nekowallStyleSheet()
{
	// Colours: a near-black with a hint of violet for the room, one step
	// lighter for the panels, and the NekoAwai pink for the one thing on
	// screen that is asking to be pressed. Nothing else is coloured -- the
	// picture is.
	return QStringLiteral(R"(
QWidget {
	background: #15121c;
	color: #e9e4f2;
	font-size: 10.5pt;
}

QLabel#preview {
	background: #0d0b12;
	border: 1px solid #2a2438;
	border-radius: 10px;
}

QLabel#credit {
	font-size: 11pt;
}

QLabel#status, QLabel#hint {
	color: #9a92ad;
}

QLabel#section {
	color: #cfc7e0;
	font-weight: 600;
	padding-top: 4px;
}

a {
	color: #f083b6;
	text-decoration: none;
}

QTabWidget::pane {
	border: none;
	top: -1px;
}

QTabBar::tab {
	background: transparent;
	color: #9a92ad;
	padding: 8px 18px;
	margin-right: 4px;
	border-bottom: 2px solid transparent;
}

QTabBar::tab:selected {
	color: #ffffff;
	border-bottom: 2px solid #e05a95;
}

QTabBar::tab:hover:!selected {
	color: #d8d1e6;
}

QPushButton {
	background: #221d2e;
	border: 1px solid #322b42;
	border-radius: 8px;
	padding: 8px 16px;
}

QPushButton:hover {
	background: #2a2338;
	border-color: #453a5c;
}

QPushButton:pressed {
	background: #1c1826;
}

QPushButton:disabled {
	color: #5f596e;
	background: #1a1622;
}

QPushButton#primary {
	background: #e05a95;
	border: none;
	color: #17101a;
	font-weight: 600;
}

QPushButton#primary:hover {
	background: #ec6ea5;
}

QPushButton#primary:disabled {
	background: #4a2c3c;
	color: #8b7482;
}

/* Tags and the choices in a row are the same pill, one that stays pressed. */
QPushButton[pill="true"] {
	background: #1c1826;
	border: 1px solid #2f2840;
	border-radius: 14px;
	padding: 6px 14px;
	color: #b8b0c8;
	text-align: center;
}

QPushButton[pill="true"]:hover {
	border-color: #4b3f63;
	color: #ded7ea;
}

QPushButton[pill="true"]:checked {
	background: #e05a95;
	border-color: #e05a95;
	color: #17101a;
	font-weight: 600;
}

QPushButton[pill="true"]:disabled {
	color: #4f4a5e;
	background: #171320;
	border-color: #221d2e;
}

/* A blocked tag is not a wish, it is a refusal: it wears a different colour
   when it is on, so the two grids can never be read as the same thing. */
QPushButton[pill="block"]:checked {
	background: #3a3350;
	border-color: #574a78;
	color: #efeaf8;
	font-weight: 600;
}

QSpinBox {
	background: #1c1826;
	border: 1px solid #2f2840;
	border-radius: 8px;
	padding: 5px 8px;
	min-width: 72px;
}

QScrollArea, QScrollArea > QWidget > QWidget {
	background: transparent;
}

QScrollBar:vertical {
	background: transparent;
	width: 10px;
	margin: 0;
}

QScrollBar::handle:vertical {
	background: #322b42;
	border-radius: 5px;
	min-height: 40px;
}

QScrollBar::handle:vertical:hover {
	background: #453a5c;
}

QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page {
	background: none;
	border: none;
	height: 0;
}
)");
}
