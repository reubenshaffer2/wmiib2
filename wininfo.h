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
#ifndef WININFO_H
#define WININFO_H

#include <QObject>
#include <QPixmap>
#include <xcb/xcb.h>

class WinInfo : public QObject
{
    Q_OBJECT
public:
    explicit WinInfo(xcb_window_t win_id, const QString &title = QString("(unknown)"), QObject *parent = nullptr);
    ~WinInfo();
    QPixmap GetPixmap(bool updatenwp = false);
    QString GetTitle() const;

signals:

public slots:
    void SetTitle(const QString &newtit);
    void UpdatePixmap();

private:
    xcb_connection_t *connection;
    xcb_window_t xcb_win;
    xcb_pixmap_t xcb_pm;
    xcb_visualid_t xcb_vis;
    uint16_t win_width, win_height;
    uint8_t win_depth;
    QString win_title;
    bool pm_alloced;

};

#endif // WININFO_H
