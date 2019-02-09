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
#ifndef XCBEVENTFILTER_H
#define XCBEVENTFILTER_H

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QList>
#include <QMap>
#include <QSize>
#include <QString>
#include <QMutex>
#include <xcb/xcb.h>

class xcbEventFilter : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit xcbEventFilter();
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *) override;
    void GetClientListUpdate(xcb_window_t rootwin);
    static xcb_timestamp_t GetUserTime();
    static bool errorHandler(const QString &prefix, xcb_generic_error_t **errp);

signals:
    void WindowMapped(xcb_window_t, QString);
    void WindowDestroyed(xcb_window_t);
    void WindowIconified(xcb_window_t);
    void WindowDamaged(xcb_window_t);
    void WindowTitleChanged(xcb_window_t, QString);
    void WindowResized(xcb_window_t, QSize);

public slots:
    void Startup();

private:
    class client_info
    {
        friend class xcbEventFilter;
    public:
        client_info();
        client_info(const client_info &other);
    private:
        client_info(xcb_window_t win);
        bool GetTitle();
        bool GetFrame();
        void GetState();
        void GetFrameState();
        void GetWindowType();
        bool IsIconified() const;
        QString title;
        QSize size;
        xcb_window_t window, frame;
        bool state_hidden, state_shaded, frame_state_hidden, wtype_no_skip;
        xcb_connection_t *connection;
    };
    xcb_window_t ClientForFrame(xcb_window_t win);
    xcb_connection_t *connection;
    QList<xcb_window_t> root_wins;
    QMap<xcb_window_t, client_info> clients;
    static QMutex ut_mutex;
    static xcb_timestamp_t user_time;
};

#endif // XCBEVENTFILTER_H
