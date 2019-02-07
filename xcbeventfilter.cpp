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
#include "xcbeventfilter.h"
#include "atomcache.h"
#include <xcb/xcb_event.h>
#include <xcb/damage.h>
#include <QX11Info>
#include <QQueue>
#include <QDebug>

QMutex xcbEventFilter::ut_mutex;
xcb_timestamp_t xcbEventFilter::user_time = 1L;

xcbEventFilter::xcbEventFilter() : QObject(nullptr)
{
    connection = QX11Info::connection();
}

void xcbEventFilter::Startup()
{
    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t screen_iter = xcb_setup_roots_iterator(setup);
    int screen_count = xcb_setup_roots_length(setup);
    xcb_screen_t *screen;
    // get root window client lists and set event masks
    // trying to go for a slightly smaller set of events here but we may need more
    uint32_t mask[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE };
    for (int i = 0; i < screen_count; ++i) {
        screen = screen_iter.data;
        xcb_change_window_attributes(connection, screen->root, XCB_CW_EVENT_MASK, mask);
        root_wins.append(screen->root);
        GetClientListUpdate(screen->root);
        xcb_screen_next(&screen_iter);
    }
    xcb_flush(connection);
}

void xcbEventFilter::GetClientListUpdate(xcb_window_t rootwin)
{
    static xcb_atom_t net_client_list = AtomCache::GetAtom("_NET_CLIENT_LIST");
    // this mask may be excessive
    //static uint32_t mask[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE };
    // let's try to lighten up...
    // we get substructurenotify events from root anyway so no need for structurenotify here
    // we never needed substructurenotify from what i can tell
    // so just property change, and really only for title/state
    static uint32_t mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_get_property_cookie_t gp_cookie = xcb_get_property(connection, 0, rootwin, net_client_list, XCB_ATOM_WINDOW, 0, BUFSIZ);
    xcb_get_property_reply_t *prop_reply;
    xcb_generic_error_t *err;
    QList<xcb_window_t> newcl;
    if ((prop_reply = xcb_get_property_reply(connection, gp_cookie, &err)))
    {
        if (prop_reply->type != XCB_ATOM_NONE)
        {
            xcb_window_t *client_list = (xcb_window_t *)xcb_get_property_value(prop_reply);
            int num_children = prop_reply->value_len;
            for (int i = 0; i < num_children; ++i) newcl.append(client_list[i]);
        }
        free(prop_reply);
    }
    else
    {
        qDebug() << "MenuXcbEventFilter: error getting client list replies";
        switch (err->error_code)
        {
        case XCB_VALUE: qDebug() << "BadValue"; break;
        case XCB_WINDOW: qDebug() << "BadWindow"; break;
        case XCB_ATOM: qDebug() << "BadAtom"; break;
        default: qDebug() << "UnknownError " << err->error_code;
        }
        xcb_value_error_t *verr = (xcb_value_error_t *)err;
        qDebug() << "  Sequence:     " << verr->sequence;
        qDebug() << "  Bad Value:    " << verr->bad_value;
        qDebug() << "  Minor OpCode: " << verr->minor_opcode;
        qDebug() << "  Major OpCode: " << verr->major_opcode;
        qDebug() << "  Pad0:         " << verr->pad0;
        free(err);
    }
    // find any new clients
    for (int i = 0; i < newcl.count(); ++i)
    {
        xcb_window_t new_client = newcl.at(i);
        if (!clients.contains(new_client))
        {
            // add to clients
            client_info nc_info(new_client);
            clients[new_client] = nc_info;
            // request more events
            xcb_get_window_attributes_cookie_t gwa_cookie = xcb_get_window_attributes(connection, new_client);
            // add to mask; don't replace it.
            xcb_get_window_attributes_reply_t *gwa_reply = xcb_get_window_attributes_reply(connection, gwa_cookie, &err);
            if (gwa_reply)
            {
                if ((gwa_reply->your_event_mask & mask[0]) != mask[0])
                {
                    uint32_t newmask[] = { mask[0] };
                    newmask[0] |= gwa_reply->your_event_mask;
                    xcb_change_window_attributes(connection, new_client, XCB_CW_EVENT_MASK, newmask);
                }
                free(gwa_reply);
            }
            if (nc_info.wtype_no_skip)
            {
                // emit signal
                emit WindowMapped(new_client, clients[new_client].title);
                // and maybe another
                if (clients[new_client].IsIconified()) emit WindowIconified(new_client);
            }
        }
    }
    // find old clients that are gone
    QMap<xcb_window_t, client_info>::iterator p;
    QQueue<xcb_window_t> removed;
    for (p = clients.begin(); p != clients.end(); ++p)
    {
        xcb_window_t old_client = p.key();
        if (!newcl.contains(old_client)) removed.enqueue(old_client);
    }
    while (!removed.empty())
    {
        xcb_window_t old_client = removed.dequeue();
        bool do_emit = clients[old_client].wtype_no_skip;
        clients.remove(old_client);
        if (do_emit) emit WindowDestroyed(old_client);
    }
}

bool xcbEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *)
{
    static xcb_atom_t net_wm_user_time = AtomCache::GetAtom("_NET_WM_USER_TIME");
    static xcb_atom_t net_client_list = AtomCache::GetAtom("_NET_CLIENT_LIST");
    static xcb_atom_t net_wm_name = AtomCache::GetAtom("_NET_WM_NAME");
    static xcb_atom_t wm_name = AtomCache::GetAtom("WM_NAME");
    static xcb_atom_t net_frame_window = AtomCache::GetAtom("_NET_FRAME_WINDOW");
    static xcb_atom_t net_wm_state = AtomCache::GetAtom("_NET_WM_STATE");
    static xcb_atom_t net_wm_window_type = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE");
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *ev = static_cast<xcb_generic_event_t *>(message);
        xcb_map_notify_event_t *map_notify_ev;
        xcb_unmap_notify_event_t *unmap_notify_ev;
        xcb_configure_notify_event_t *configure_notify_ev;
        xcb_property_notify_event_t *property_notify_ev;
        xcb_damage_notify_event_t *damage_notify_ev;
        xcb_window_t cff_win;

        switch (ev->response_type & ~0x80)
        {
        case XCB_UNMAP_NOTIFY:
            unmap_notify_ev = (xcb_unmap_notify_event_t *)ev;
            //qDebug() << "xcbEventFilter unmap event " << QString("0x%1").arg(unmap_notify_ev->window, 0, 16);
            // we're only concerned about frames because we're tracking _NET_WM_STATE
            if ((cff_win = ClientForFrame(unmap_notify_ev->window)))
            {
                bool wasIcon = clients[cff_win].IsIconified();
                clients[cff_win].GetFrameState();
                if ((!wasIcon) && clients[cff_win].IsIconified())
                {
                    if (clients[cff_win].wtype_no_skip) emit WindowIconified(cff_win);
                }
            }
            break;
        case XCB_MAP_NOTIFY:
            map_notify_ev = (xcb_map_notify_event_t *)ev;
            // we're only concerned about frames because we're tracking _NET_WM_STATE
            if ((cff_win = ClientForFrame(map_notify_ev->window)))
            {
                bool wasIcon = clients[cff_win].IsIconified();
                clients[cff_win].GetFrameState();
                if (wasIcon && (!clients[cff_win].IsIconified()))
                {
                    if (clients[cff_win].wtype_no_skip) emit WindowMapped(cff_win, clients[cff_win].title);
                }
            }
            break;
        case XCB_CONFIGURE_NOTIFY:
            configure_notify_ev = (xcb_configure_notify_event_t *)ev;
            if (clients.contains(configure_notify_ev->window))
            {
                QSize size(configure_notify_ev->width, configure_notify_ev->height);
                if (size != clients[configure_notify_ev->window].size)
                {
                    clients[configure_notify_ev->window].size = size;
                    if (clients[configure_notify_ev->window].wtype_no_skip) emit WindowResized(configure_notify_ev->window, size);
                }
            }
            break;
        case XCB_PROPERTY_NOTIFY:
            property_notify_ev = (xcb_property_notify_event_t *)ev;
            if (root_wins.contains(property_notify_ev->window))
            {
                // The only root property I care about is _NET_CLIENT_LIST.
                if (property_notify_ev->atom == net_client_list) GetClientListUpdate(property_notify_ev->window);
            }
            else if (clients.contains(property_notify_ev->window))
            {
                // title change
                if (property_notify_ev->atom == net_wm_name || property_notify_ev->atom == wm_name)
                {
                    if (clients[property_notify_ev->window].GetTitle() && clients[property_notify_ev->window].wtype_no_skip)
                    {
                        emit WindowTitleChanged(property_notify_ev->window, clients[property_notify_ev->window].title);
                    }
                }
                // state change
                else if (property_notify_ev->atom == net_wm_state)
                {
                    bool wasIcon = clients[property_notify_ev->window].IsIconified();
                    clients[property_notify_ev->window].GetState();
                    if (clients[property_notify_ev->window].wtype_no_skip)
                    {
                        if (!wasIcon)
                        {
                            if (clients[property_notify_ev->window].IsIconified()) emit WindowIconified(property_notify_ev->window);
                        }
                        else
                        {
                            if (!clients[property_notify_ev->window].IsIconified()) emit WindowMapped(property_notify_ev->window, clients[property_notify_ev->window].title);
                        }
                    }
                }
                // frame change
                else if (property_notify_ev->atom == net_frame_window)
                {
                    // frame added/removed should not change anything else of interest
                    clients[property_notify_ev->window].GetFrame();
                    clients[property_notify_ev->window].GetFrameState();
                }
                // window type
                else if (property_notify_ev->atom == net_wm_window_type)
                {
                    // window type changed
                    bool old_wtype_nn = clients[property_notify_ev->window].wtype_no_skip;
                    clients[property_notify_ev->window].GetWindowType();
                    if (clients[property_notify_ev->window].wtype_no_skip)
                    {
                        if (!old_wtype_nn) emit WindowMapped(property_notify_ev->window, clients[property_notify_ev->window].title);
                    }
                    else
                    {
                        if (old_wtype_nn) emit WindowDestroyed(property_notify_ev->window);
                    }
                }
            }
            else if (property_notify_ev->atom == net_wm_user_time)
            {
                ut_mutex.lock();
                user_time = property_notify_ev->time;
                ut_mutex.unlock();
            }
            break;
        case XCB_DAMAGE_NOTIFY:
            damage_notify_ev = (xcb_damage_notify_event_t *)ev;
            if (clients.contains(damage_notify_ev->drawable))
            {
                // toplevel window damage reported
                if (clients[damage_notify_ev->drawable].wtype_no_skip) emit WindowDamaged(damage_notify_ev->drawable);
            }
            break;
        default:
            //qDebug() << "Unknown Event " << (ev->response_type & ~0x80) << AtomCache::GetAtomName(ev->response_type & ~0x80);
            break;
        }
    }
    return false;
}

xcb_timestamp_t xcbEventFilter::GetUserTime()
{
    ut_mutex.lock();
    xcb_timestamp_t ret = user_time;
    ut_mutex.unlock();
    return ret;
}

xcb_window_t xcbEventFilter::ClientForFrame(xcb_window_t win)
{
    QMap<xcb_window_t, client_info>::iterator p;
    for (p = clients.begin(); p != clients.end(); ++p)
    {
        if (p.value().frame == win) return p.key();
    }
    return 0UL;
}


// ----------------( client_info )-------------------

xcbEventFilter::client_info::client_info()
{
    window = 0UL;
    wtype_no_skip = false;
}

xcbEventFilter::client_info::client_info(const client_info &other) :
    xcbEventFilter::client_info::client_info(other.window) { }

xcbEventFilter::client_info::client_info(xcb_window_t win)
{
    window = win;
    if (!window) return;
    connection = QX11Info::connection();
    xcb_generic_error_t *err = nullptr;
    xcb_get_geometry_cookie_t gg_cookie = xcb_get_geometry(connection, win);
    xcb_get_geometry_reply_t *gg_reply = xcb_get_geometry_reply(connection, gg_cookie, &err);
    if (gg_reply)
    {
        size = QSize(gg_reply->width, gg_reply->height);
        free(gg_reply);
    }
    if (err)
    {
        qDebug() << "client_info::client_info():get_geometry XCB Error " << err->error_code;
        free(err);
    }
    GetTitle();
    GetState();
    GetFrame();
    GetFrameState();
    GetWindowType();
}

bool xcbEventFilter::client_info::GetTitle()
{
    static xcb_atom_t net_wm_name = AtomCache::GetAtom("_NET_WM_NAME");
    static xcb_atom_t wm_name = AtomCache::GetAtom("WM_NAME");
    static xcb_atom_t utf8_string = AtomCache::GetAtom("UTF8_STRING");
    if (!window) return false;
    xcb_generic_error_t *err = nullptr;
    xcb_get_property_cookie_t gp_cookie;
    xcb_get_property_reply_t *gp_reply;
    QString newTitle;
    gp_cookie = xcb_get_property(connection, 0, window, net_wm_name, utf8_string, 0, BUFSIZ);
    gp_reply = xcb_get_property_reply(connection, gp_cookie, &err);
    if (gp_reply)
    {
        if (gp_reply->type == utf8_string)
        {
            const char *gp_utf8str = (const char *)xcb_get_property_value(gp_reply);
            int gp_utf8str_len = xcb_get_property_value_length(gp_reply);
            newTitle = QString::fromUtf8(gp_utf8str, gp_utf8str_len);
        }
        free(gp_reply);
    }
    if (err)
    {
        qDebug() << "client_info::GetTitle:get property _NET_WM_NAME XCB Error " << err->error_code;
        free(err);
        err = nullptr;
    }
    if (!newTitle.isEmpty())
    {
        if (newTitle != title)
        {
            title = newTitle;
            return true;
        }
        else return false;
    }
    gp_cookie = xcb_get_property(connection, 0, window, wm_name, XCB_ATOM_ANY, 0, BUFSIZ);
    gp_reply = xcb_get_property_reply(connection, gp_cookie, &err);
    if (gp_reply)
    {
        if (gp_reply->type != XCB_ATOM_NONE)
        {
            const char *gp_utf8str = (const char *)xcb_get_property_value(gp_reply);
            int gp_utf8str_len = xcb_get_property_value_length(gp_reply);
            newTitle = QString::fromUtf8(gp_utf8str, gp_utf8str_len);
        }
        free(gp_reply);
    }
    if (err)
    {
        qDebug() << "client_info::GetTitle:get property WM_NAME XCB Error " << err->error_code;
        free(err);
        err = nullptr;
    }
    if (!newTitle.isEmpty())
    {
        if (newTitle != title)
        {
            title = newTitle;
            return true;
        }
        else return false;
    }
    if (title != "(no name)")
    {
        title = "(no name)";
        return true;
    }
    return false;
}

bool xcbEventFilter::client_info::GetFrame()
{
    static xcb_atom_t net_frame_window = AtomCache::GetAtom("_NET_FRAME_WINDOW");
    if (!window) return false;
    xcb_generic_error_t *err = nullptr;
    xcb_window_t newFrame = 0UL;
    xcb_get_property_cookie_t gp_cookie = xcb_get_property(connection, 0, window, net_frame_window, XCB_ATOM_WINDOW, 0, BUFSIZ);
    xcb_get_property_reply_t *gp_reply = xcb_get_property_reply(connection, gp_cookie, &err);
    if (gp_reply)
    {
        if (gp_reply->type == XCB_ATOM_WINDOW) newFrame = *((xcb_window_t *)xcb_get_property_value(gp_reply));
        free(gp_reply);
    }
    if (err)
    {
        qDebug() << "client_info::GetFrame:get property _NET_FRAME_WINDOW XCB Error " << err->error_code;
        free(err);
        err = nullptr;
    }
    if (newFrame != frame)
    {
        frame = newFrame;
        return true;
    }
    return false;
}

void xcbEventFilter::client_info::GetState()
{
    static xcb_atom_t net_wm_state = AtomCache::GetAtom("_NET_WM_STATE");
    static xcb_atom_t net_wm_state_hidden = AtomCache::GetAtom("_NET_WM_STATE_HIDDEN");
    static xcb_atom_t net_wm_state_shaded = AtomCache::GetAtom("_NET_WM_STATE_SHADED");
    if (!window) return;
    xcb_generic_error_t *err = nullptr;
    xcb_get_property_cookie_t gp_cookie = xcb_get_property(connection, 0, window, net_wm_state, XCB_ATOM_ATOM, 0, BUFSIZ);
    xcb_get_property_reply_t *gp_reply = xcb_get_property_reply(connection, gp_cookie, &err);
    if (gp_reply)
    {
        state_hidden = false;
        state_shaded = false;
        if (gp_reply->type == XCB_ATOM_ATOM)
        {
            xcb_atom_t *state_atoms = (xcb_atom_t *)xcb_get_property_value(gp_reply);
            int sa_len = gp_reply->value_len; // xcb_get_property_value_length(gp_reply);
            for (int i = 0; i < sa_len; ++i)
            {
                if (state_atoms[i] == net_wm_state_hidden) state_hidden = true;
                if (state_atoms[i] == net_wm_state_shaded) state_shaded = true;
            }
        }
        free(gp_reply);
    }
    if (err)
    {
        qDebug() << "client_info::GetState: XCB Error " << err->error_code;
        free(err);
        err = nullptr;
    }
}

void xcbEventFilter::client_info::GetFrameState()
{
    // you don't actually get net_wm_state for the frame.
    // you actually get the window attributes and look at the map state
    if (!frame) return;
    xcb_generic_error_t *err = nullptr;
    xcb_get_window_attributes_cookie_t ga_cookie = xcb_get_window_attributes(connection, frame);
    xcb_get_window_attributes_reply_t *ga_reply = xcb_get_window_attributes_reply(connection, ga_cookie, &err);
    if (ga_reply)
    {
        frame_state_hidden = (ga_reply->map_state != XCB_MAP_STATE_VIEWABLE);
        free(ga_reply);
    }
    if (err)
    {
        qDebug() << "client_info::GetFrameState: XCB Error " << err->error_code;
        free(err);
        err = nullptr;
    }
}

void xcbEventFilter::client_info::GetWindowType()
{
    static xcb_atom_t net_wm_window_type = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE");
    static xcb_atom_t net_wm_window_type_desktop = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_DESKTOP");
    static xcb_atom_t net_wm_window_type_dock = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_DOCK");
    static xcb_atom_t net_wm_window_type_toolbar = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_TOOLBAR");
    static xcb_atom_t net_wm_window_type_menu = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_MENU");
    static xcb_atom_t net_wm_window_type_utility = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_UTILITY");
    static xcb_atom_t net_wm_window_type_splash = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_SPLASH");
    static xcb_atom_t net_wm_window_type_dialog = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_DIALOG");
    //static xcb_atom_t net_wm_window_type_normal = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");
    if (!window) return;
    xcb_generic_error_t *err = nullptr;
    xcb_get_property_cookie_t gp_cookie = xcb_get_property(connection, 0, window, net_wm_window_type, XCB_ATOM_ATOM, 0, BUFSIZ);
    xcb_get_property_reply_t *gp_reply = xcb_get_property_reply(connection, gp_cookie, &err);
    if (gp_reply)
    {
        wtype_no_skip = true;
        if (gp_reply->type == XCB_ATOM_ATOM)
        {
            xcb_atom_t *wtype_atoms = (xcb_atom_t *)xcb_get_property_value(gp_reply);
            int wa_len = gp_reply->value_len; //xcb_get_property_value_length(gp_reply);
            for (int i = 0; i < wa_len; ++i)
            {
                //qDebug() << "window " << window << " type " << AtomCache::GetAtomName(wtype_atoms[i]);
                if (wtype_atoms[i] == net_wm_window_type_desktop
                        || wtype_atoms[i] == net_wm_window_type_dock
                        || wtype_atoms[i] == net_wm_window_type_toolbar
                        || wtype_atoms[i] == net_wm_window_type_menu
                        || wtype_atoms[i] == net_wm_window_type_utility
                        || wtype_atoms[i] == net_wm_window_type_splash
                        || wtype_atoms[i] == net_wm_window_type_dialog)
                {
                    wtype_no_skip = false;
                }
                //if (wtype_atoms[i] != net_wm_window_type_normal) wtype_normal_null = false;
            }
        }
        free(gp_reply);
    }
    if (err)
    {
        qDebug() << "client_info::GetWindowType: XCB Error " << err->error_code;
        free(err);
        err = nullptr;
    }
}

bool xcbEventFilter::client_info::IsIconified() const
{
    if (!window) return false;
    return ((frame && frame_state_hidden) || ((!frame) && state_hidden));
}


