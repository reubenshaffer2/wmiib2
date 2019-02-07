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
#include <cstdlib>
#include "atomcache.h"
#include <QX11Info>
#include <QDebug>

class AtomNode
{
public:
    AtomNode() : name("NONE"), atom(0) { }
    AtomNode(const QString &new_name, xcb_atom_t new_atom) : name(new_name), atom(new_atom) { }
    QString name;
    xcb_atom_t atom;
};

xcb_connection_t *AtomCache::connection(nullptr);
QList<AtomNode> AtomCache::atom_cache;

xcb_atom_t AtomCache::GetAtom(const QString &name) {
    QList<AtomNode>::iterator p;
    xcb_atom_t ret = XCB_ATOM_NONE;
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;
    //xcb_connection_t *connection = QX11Info::connection();
    if (!connection) connection = QX11Info::connection();
    xcb_generic_error_t *err = nullptr;

    for (p = atom_cache.begin(); p != atom_cache.end(); ++p) {
        if (name == p->name) {
            ret = p->atom;
            break;
        }
    }
    if (ret == XCB_ATOM_NONE) {
        QByteArray utf8Name = name.toUtf8();
        atom_cookie = xcb_intern_atom(connection, 0, utf8Name.length(), utf8Name.constData());
        //qDebug() << "request atom " << utf8Name;
        if ((atom_reply = xcb_intern_atom_reply(connection, atom_cookie, &err))) {
            //qDebug() << "atom reply " << atom_reply->atom;
            if (err) qDebug() << "AtomCache::GetAtom: XCB Error: " << err->error_code;
            if (err) free(err);
            ret = atom_reply->atom;
            atom_cache.append(AtomNode(name, ret));
            free(atom_reply);
        }
    }
    return ret;
}

QString AtomCache::GetAtomName(xcb_atom_t atom) {
    QList<AtomNode>::iterator p;
    QString ret;
    int name_len;
    xcb_get_atom_name_cookie_t atom_cookie;
    xcb_get_atom_name_reply_t *atom_reply;
    //xcb_connection_t *connection = QX11Info::connection();
    if (!connection) connection = QX11Info::connection();
    xcb_generic_error_t *err = nullptr;

    for (p = atom_cache.begin(); p != atom_cache.end(); ++p) {
        if (atom == p->atom) {
            ret = p->name;
            break;
        }
    }
    if (ret.isEmpty()) {
        atom_cookie = xcb_get_atom_name(connection, atom);
        if ((atom_reply = xcb_get_atom_name_reply(connection, atom_cookie, &err))) {
            if (err) qDebug() << "AtomCache::GetAtomName: XCB Error: " << err->error_code;
            if (err) free(err);
            name_len = xcb_get_atom_name_name_length(atom_reply);
            const char *name_cstr = xcb_get_atom_name_name(atom_reply);
            ret = QString::fromUtf8(name_cstr, name_len);
            atom_cache.append(AtomNode(ret, atom));
            free(atom_reply);
        }
    }
    return ret;
}


