/*
Copyright 2019 Reuben Robert Shaffer II.  All rights reserved.

This file is part of WMIIB2.

WMIIB2 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

WMIIB2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with WMIIB2.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QWidget>
#include <QBoxLayout>

class QSettings;

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = 0);
    ~SettingsWindow();
    QBoxLayout::Direction GetOuterLayoutDirection() const;
    QBoxLayout::Direction GetInnerLayoutDirection() const;
    Qt::Alignment GetIconAlignment() const;
    int GetIconSize() const;
    QSize GetFixedSize() const;
    bool DoesGrow() const;
    bool IsFromTop() const;
    bool IsFromLeft() const;
    bool IsTransparent() const;
    QColor GetBackgroundColor() const;

signals:
    void settingsChanged();

protected:
    void changeEvent(QEvent *e);
    void hideEvent(QHideEvent *e);
    void closeEvent(QCloseEvent *e);

private slots:
    void on_tabWidget_currentChanged(int index);
    void on_ST_Fixed_toggled(bool checked);
    void on_ST_Growing_toggled(bool checked);
    void on_Location_activated(const QString &arg1);
    void on_IconSize_valueChanged(int arg1);
    void on_TransparentBackground_toggled(bool checked);
    void on_FixedWidth_valueChanged(int arg1);
    void on_FixedHeight_valueChanged(int arg1);
    void on_GrowDirection_activated(const QString &arg1);
    void on_BGColorButton_clicked();

private:
    Ui::SettingsWindow *ui;
    QSettings *store;
    QPalette *bgColorPal;
};

#endif // SETTINGSWINDOW_H
