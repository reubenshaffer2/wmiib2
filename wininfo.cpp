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
#include "wininfo.h"
#include <QX11Info>
#include <xcb/composite.h>
#include <QDebug>
#include "xcbeventfilter.h"

WinInfo::WinInfo(xcb_window_t win_id, const QString &title, QObject *parent) :
    QObject(parent), xcb_win(win_id), win_title(title)
{
    connection = QX11Info::connection();
    xcb_generic_error_t *err = nullptr;
    xcb_get_geometry_cookie_t gg_cookie = xcb_get_geometry(connection, win_id);
    xcb_get_geometry_reply_t *gg_reply = xcb_get_geometry_reply(connection, gg_cookie, &err);
    if (gg_reply)
    {
        win_width = gg_reply->width;
        win_height = gg_reply->height;
        win_depth = gg_reply->depth;
        free(gg_reply);
    }
    xcbEventFilter::errorHandler("WinInfo::WinInfo: get_geometry: ", &err);
    xcb_pm = xcb_generate_id(connection);
    pm_alloced = false;
    UpdatePixmap();
}

WinInfo::~WinInfo()
{
    xcb_free_pixmap(connection, xcb_pm);
}

QPixmap WinInfo::GetPixmap(bool updatenwp)
{
    QPixmap ret;
    // ask compositor to associate window with pixmap
    if (updatenwp || !pm_alloced) UpdatePixmap();
    if (pm_alloced)
    {
        // now get the image
        xcb_generic_error_t *err = nullptr;
        xcb_get_image_cookie_t gi_cookie = xcb_get_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, xcb_pm, 0, 0, win_width, win_height, (uint32_t)(~0UL));
        xcb_get_image_reply_t *gi_reply = xcb_get_image_reply(connection, gi_cookie, &err);
        if (gi_reply)
        {
            int data_len = xcb_get_image_data_length(gi_reply);
            //qDebug() << "data_len = " << data_len;
            //qDebug() << "visual = " << gi_reply->visual;
            //qDebug() << "depth = " << gi_reply->depth;
            uint8_t *data = xcb_get_image_data(gi_reply);
            uchar *imgdat = (uchar*)malloc(data_len);
            if (imgdat)
            {
                memcpy(imgdat, data, data_len);
                QImage img(imgdat, win_width, win_height, QImage::Format_RGB32, free, imgdat);
                ret = QPixmap::fromImage(img);
            }
            else qDebug() << "WinInfo::GetPixmap: malloc failed for imgdat";
            free(gi_reply);
        }
        xcbEventFilter::errorHandler("WinInfo::GetPixmap: get_image: ", &err);
    }
    if (ret.isNull()) ret = QPixmap(":/resource/images/Default.png");
    return ret;
}

QString WinInfo::GetTitle() const
{
    return win_title;
}

void WinInfo::SetTitle(const QString &newtit)
{
    win_title = newtit;
}

void WinInfo::UpdatePixmap()
{
    xcb_void_cookie_t void_cookie;
    xcb_generic_error_t *err;
    if (pm_alloced)
    {
        void_cookie = xcb_free_pixmap_checked(connection, xcb_pm);
        err = xcb_request_check(connection, void_cookie);
        if (!xcbEventFilter::errorHandler("WinInfo::UpdatePixmap: free_pixmap: ", &err)) pm_alloced = false;
    }
    xcb_get_window_attributes_cookie_t ga_cookie = xcb_get_window_attributes(connection, xcb_win);
    xcb_get_window_attributes_reply_t *ga_reply = xcb_get_window_attributes_reply(connection, ga_cookie, &err);
    if (ga_reply)
    {
        if (ga_reply->map_state != XCB_MAP_STATE_UNMAPPED && !ga_reply->override_redirect)
        {
            void_cookie = xcb_composite_name_window_pixmap_checked(connection, xcb_win, xcb_pm);
            xcb_generic_error_t *err2 = xcb_request_check(connection, void_cookie);
            if (!xcbEventFilter::errorHandler("WinInfo::UpdatePixmap: name_window_pixmap: ", &err2)) pm_alloced = true;
        }
        free(ga_reply);
    }
    xcbEventFilter::errorHandler("WinInfo::UpdatePixmap: get_window_attributes: ", &err);
}
