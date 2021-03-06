/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class Ui_SettingsDialog;

namespace dialogs {

class SettingsDialog : public QDialog
{
	Q_OBJECT
	public:
		SettingsDialog(const QList<QAction*>& actions, QWidget *parent=0);
		~SettingsDialog();

	signals:
		//! Shortcuts have changed, reload them from the settings
		void shortcutsChanged() const;

	public slots:
		void rememberSettings() const;

	private slots:
		void validateShortcut(int row, int col);

	private:
		Ui_SettingsDialog *_ui;
		QList<QAction*> _customactions;
};

}

#endif

