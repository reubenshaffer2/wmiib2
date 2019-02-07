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
#ifndef WMIIB2_H
#define WMIIB2_H

#include <QWidget>
#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <QList>
#include <QMap>
#include "wininfo.h"

class QBoxLayout;
class QLabel;
class xcbEventFilter;
class SettingsWindow;

namespace Ui {
class wmiib2;
}

class wmiib2 : public QWidget
{
    Q_OBJECT

public:
    explicit wmiib2(QWidget *parent = 0);
    ~wmiib2();
    void errorHandler(const QString &prefix, xcb_generic_error_t **errp);

protected:
    void changeEvent(QEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void showEvent(QShowEvent *e);
    void resizeEvent(QResizeEvent *e);
    void paintEvent(QPaintEvent *e);

private slots:
    void winMapped(xcb_window_t win, const QString &title);
    void winDestroyed(xcb_window_t win);
    void winDamaged(xcb_window_t win);
    void winResized(xcb_window_t win, const QSize &);
    void winIconified(xcb_window_t win);
    void winTitleChanged(xcb_window_t win, const QString &title);
    void DeiconifyWindow(xcb_window_t win);
    void SettingsChanged();
    void DelayedIconCreator();

private:
    Ui::wmiib2 *ui;
    xcb_connection_t *connection;
    bool comp_version_ok, damg_version_ok;
    xcbEventFilter *evfilt;
    QMap<xcb_window_t, WinInfo *> win_info;
    QMap<xcb_window_t, QLabel *> win_icon;
    QBoxLayout *itemOuterLayout;
    QList<QBoxLayout *> itemInnerLayouts;
    SettingsWindow *setwin;
    void AdjustFrameSize(QWidgetList newWidgets = QWidgetList());
    void GenerateMask();
    void RemoveWindowIcon(xcb_window_t win);
    void AddWidgetToLayout(QLabel *newItem);
    void TryToShiftItemInLayout(int layoutIndex);
    int GetLayoutSize(int layoutIndex);
    int saved_icon_size;
    QPalette MyPalette;
    QMap<xcb_window_t, QDateTime> unmapped_wins;
    QTimer *iTimer;
};

#endif // WMIIB2_H
