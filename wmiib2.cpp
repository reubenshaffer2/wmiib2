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
#include "wmiib2.h"
#include "ui_wmiib2.h"
#include <QX11Info>
#include <QDebug>
#include <QGuiApplication>
#include <QBoxLayout>
#include <QScreen>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include "xcbeventfilter.h"
#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <QQueue>
#include "atomcache.h"
#include <QMenu>
#include <QBitmap>
#include <QPainter>
#include <QRect>
#include "settingswindow.h"
#include <QMessageBox>

wmiib2::wmiib2(QWidget *parent) :
    QWidget(parent, Qt::FramelessWindowHint),
    ui(new Ui::wmiib2)
{
    ui->setupUi(this);

    comp_version_ok = false;
    connection = QX11Info::connection();

    setWindowFlags(Qt::FramelessWindowHint);
    xcb_window_t mywin = (xcb_window_t)winId();

    xcb_atom_t net_wm_state = AtomCache::GetAtom("_NET_WM_STATE");
    xcb_atom_t net_wm_state_below = AtomCache::GetAtom("_NET_WM_STATE_BELOW");
    xcb_atom_t net_wm_state_sticky = AtomCache::GetAtom("_NET_WM_STATE_STICKY");
    xcb_atom_t net_wm_state_skip_taskbar = AtomCache::GetAtom("_NET_WM_STATE_SKIP_TASKBAR");
    xcb_atom_t net_wm_window_type = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE");
    xcb_atom_t net_wm_window_type_dock = AtomCache::GetAtom("_NET_WM_WINDOW_TYPE_DOCK");
    uint32_t prop_arr[] = {
        net_wm_window_type_dock,
        0,
        net_wm_state_below,
        net_wm_state_sticky,
        net_wm_state_skip_taskbar,
        0
    };
    xcb_change_property(connection, XCB_PROP_MODE_APPEND, mywin, net_wm_window_type, XCB_ATOM_ATOM, 32, 1, prop_arr);
    xcb_change_property(connection, XCB_PROP_MODE_APPEND, mywin, net_wm_state, XCB_ATOM_ATOM, 32, 3, &prop_arr[2]);

    xcb_generic_error_t *err = nullptr;
    xcb_composite_query_version_cookie_t comp_ver_cookie = xcb_composite_query_version(connection, 0, 2);
    xcb_composite_query_version_reply_t *comp_ver_reply = xcb_composite_query_version_reply(connection, comp_ver_cookie, &err);
    if (comp_ver_reply)
    {
        comp_version_ok = (comp_ver_reply->minor_version >= 2);
        free(comp_ver_reply);
    }
    if (err)
    {
        qDebug() << "wmiib2::wmiib2: query composite version XCB error: " << err->error_code;
        free(err);
        err = nullptr;
    }
    damg_version_ok = false;
    xcb_damage_query_version_cookie_t damg_ver_cookie = xcb_damage_query_version(connection, 1, 1);
    xcb_damage_query_version_reply_t *damg_ver_reply = xcb_damage_query_version_reply(connection, damg_ver_cookie, &err);
    if (damg_ver_reply)
    {
        damg_version_ok = (damg_ver_reply->major_version >= 1 && damg_ver_reply->minor_version >= 1);
        free(damg_ver_reply);
    }
    if (err)
    {
        qDebug() << "wmiib2::wmiib2: query damage version XCB error: " << err->error_code;
        free(err);
        err = nullptr;
    }
    if (!(comp_version_ok && damg_version_ok)) return;
    // create settings window and connect to slot
    setwin = new SettingsWindow;
    connect(setwin, SIGNAL(settingsChanged()), this, SLOT(SettingsChanged()));
    MyPalette.setColor(QPalette::Base, setwin->GetBackgroundColor());
    MyPalette.setColor(QPalette::Window, setwin->GetBackgroundColor());
    setPalette(MyPalette);
    saved_icon_size = setwin->GetIconSize();
    // set up the layouts that will hold our icons
    ui->horizontalLayout->setContentsMargins(2, 2, 2, 2);
    itemOuterLayout = new QBoxLayout(setwin->GetOuterLayoutDirection(), ui->frame);
    itemOuterLayout->setMargin(0);
    itemOuterLayout->setContentsMargins(3, 3, 3, 3);
    itemOuterLayout->setSpacing(10);
    ui->frame->setLayout(itemOuterLayout);
    QBoxLayout *innerLayout = new QBoxLayout(setwin->GetInnerLayoutDirection());
    innerLayout->setMargin(0);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(10);
    itemOuterLayout->addLayout(innerLayout);
    itemOuterLayout->setAlignment(innerLayout, setwin->GetIconAlignment());
    itemInnerLayouts.append(innerLayout);
    // create, set up, and install event filter.
    evfilt = new xcbEventFilter;
    connect(evfilt, SIGNAL(WindowMapped(xcb_window_t,QString)), this, SLOT(winMapped(xcb_window_t,QString)));
    connect(evfilt, SIGNAL(WindowDestroyed(xcb_window_t)), this, SLOT(winDestroyed(xcb_window_t)));
    connect(evfilt, SIGNAL(WindowIconified(xcb_window_t)), this, SLOT(winIconified(xcb_window_t)));
    connect(evfilt, SIGNAL(WindowDamaged(xcb_window_t)), this, SLOT(winDamaged(xcb_window_t)));
    connect(evfilt, SIGNAL(WindowResized(xcb_window_t,QSize)), this, SLOT(winResized(xcb_window_t,QSize)));
    connect(evfilt, SIGNAL(WindowTitleChanged(xcb_window_t,QString)), this, SLOT(winTitleChanged(xcb_window_t,QString)));
    evfilt->Startup();
    qGuiApp->installNativeEventFilter(evfilt);
    AdjustFrameSize();
}

wmiib2::~wmiib2()
{
    delete ui;
}

void wmiib2::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void wmiib2::mousePressEvent(QMouseEvent *e)
{
    QWidget *wat = ui->frame->childAt(ui->frame->mapFrom(this, e->pos()));
    if (e->button() == Qt::LeftButton && wat)
    {
        QMap<xcb_window_t, QLabel *>::iterator p;
        for (p = win_icon.begin(); p != win_icon.end(); ++p)
        {
            if (p.value() == wat)
            {
                DeiconifyWindow(p.key());
                e->accept();
            }
        }
    }
    else
    {
        QMenu *ibmenu = new QMenu(this);
        ibmenu->addAction(QString("Close IconBox"), qApp, SLOT(quit()));
        ibmenu->addAction(QString("IconBox Settings"), setwin, SLOT(showNormal()));
        ibmenu->popup(e->globalPos());
        e->accept();
    }
    e->ignore();
}

void wmiib2::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!(comp_version_ok && damg_version_ok))
    {
        QMessageBox::critical(this, "IconBox (wmiib2)", "<p>This application requires you to have the <strong>composite</strong> and <strong>damage</strong> extensions installed and enabled on the X11 server.  Please enable these extensions, restart your X11 server, and try again.</p>", QMessageBox::Close, QMessageBox::NoButton);
        qApp->quit();
    }
}

void wmiib2::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
}

void wmiib2::paintEvent(QPaintEvent *e)
{
    GenerateMask();
    QWidget::paintEvent(e);
}

void wmiib2::winMapped(xcb_window_t win, const QString &title)
{
    //qDebug() << "wmiib2::winMapped(" << win << ", " << title << ")";
    if (!(win_info.contains(win) && win_info[win]))
    {
        xcb_void_cookie_t void_cookie = xcb_composite_redirect_window_checked(connection, win, XCB_COMPOSITE_REDIRECT_AUTOMATIC);
        xcb_generic_error_t *err = xcb_request_check(connection, void_cookie);
        if (err)
        {
            qDebug() << "wmiib2::winMapped: composite_redirect_window XCB error " << err->error_code;
            free(err);
        }
        win_info[win] = new WinInfo(win, title);
    }
    else
    {
        win_info[win]->SetTitle(title);
        win_info[win]->UpdatePixmap();
    }
    RemoveWindowIcon(win);
}

void wmiib2::winDestroyed(xcb_window_t win)
{
    //qDebug() << "wmiib2::winDestroyed(" << win << ")";
    if (win_info.contains(win))
    {
        if (win_info[win]) win_info[win]->deleteLater();
        win_info.remove(win);
    }
    RemoveWindowIcon(win);
}

void wmiib2::winDamaged(xcb_window_t win)
{
    //qDebug() << "wmiib2::winDamaged(" << win << ")";
    // only worry about iconified windows that are damaged
    // this makes no sense.  iconified windows are unmapped and can't be damaged.
    if (win_icon.contains(win) && win_icon[win] && win_info.contains(win) && win_info[win])
    {
        win_icon[win]->setPixmap(win_info[win]->GetPixmap(true));
    }
}

void wmiib2::winResized(xcb_window_t win, const QSize &)
{
    //qDebug() << "wmiib2::winResized(" << win << ", " << newSize << ")";
    if (win_info.contains(win) && win_info[win])
    {
        win_info[win]->UpdatePixmap();
    }
}

void wmiib2::winIconified(xcb_window_t win)
{
    //qDebug() << "wmiib2::winIconified(" << win << ")";
    // should always be true
    if (win_info.contains(win) && win_info[win])
    {
        if (!(win_icon.contains(win) && win_icon[win]))
        {
            // should always be true
            QLabel *newItem = new QLabel(ui->frame);
            QPixmap win_pm = win_info[win]->GetPixmap(false).scaled(saved_icon_size, saved_icon_size, Qt::KeepAspectRatio);
            newItem->setPixmap(win_pm);
            newItem->setFixedSize(win_pm.size());
            newItem->setToolTip(win_info[win]->GetTitle());
            // find the place to insert into layouts
            AddWidgetToLayout(newItem);
            win_icon[win] = newItem;
            AdjustFrameSize();
        }
    }
}

void wmiib2::winTitleChanged(xcb_window_t win, const QString &title)
{
    //qDebug() << "wmiib2::winTitleChanged(" << win << ", " << title << ")";
    if (win_info.contains(win) && win_info[win]) win_info[win]->SetTitle(title);
    if (win_icon.contains(win) && win_icon[win]) win_icon[win]->setToolTip(title);
}

void wmiib2::DeiconifyWindow(xcb_window_t win)
{
    //qDebug() << "wmiib2::DeiconifyWindow(" << win << ")";
    static xcb_atom_t net_active_window = AtomCache::GetAtom("_NET_ACTIVE_WINDOW");

    xcb_query_tree_cookie_t qtree_cookie = xcb_query_tree(connection, win);
    xcb_generic_error_t *err = nullptr;
    xcb_query_tree_reply_t *qtree_reply = xcb_query_tree_reply(connection, qtree_cookie, &err);
    if (qtree_reply)
    {
        xcb_window_t rootwin = qtree_reply->root;
        // map the window
        xcb_map_window(connection, win);
        // raise the window
        uint32_t values[] = { XCB_STACK_MODE_ABOVE };
        xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
        // make it the active window
        xcb_client_message_event_t client_message_event;
        client_message_event.response_type = XCB_CLIENT_MESSAGE;
        client_message_event.format = 32;
        client_message_event.sequence = 0;
        client_message_event.window = win;
        client_message_event.type = net_active_window;
        client_message_event.data.data32[0] = 2UL; // am i a pager (2) or an application (1)?
        client_message_event.data.data32[1] = xcbEventFilter::GetUserTime();
        client_message_event.data.data32[2] = (uint32_t)winId();
        client_message_event.data.data32[3] = 0UL;
        client_message_event.data.data32[4] = 0UL;
        xcb_send_event(connection, 1, rootwin, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char *)&client_message_event);
        xcb_flush(connection);
        free(qtree_reply);
    }
    if (err)
    {
        qDebug() << "wmiib2::DeiconifyWindow: XCB Error " << err->error_code;
        free(err);
    }
}

void wmiib2::AdjustFrameSize()
{
    //qDebug() << "wmiib2::AdjustFrameSize()";
    int icon_size = setwin->GetIconSize();
    QBoxLayout::Direction direction = setwin->GetInnerLayoutDirection();
    QSize screen_size = QGuiApplication::primaryScreen()->size();
    // minimum
    int newWidth = 0;
    int newHeight = 0;
    if (setwin->DoesGrow())
    {
        if (direction == QBoxLayout::TopToBottom || direction == QBoxLayout::BottomToTop)
        {
            // vertical, then horizontal
            for (int i = 0; i < itemInnerLayouts.count(); ++i)
            {
                int colHeight = 0;
                int colWidth = 0;
                QBoxLayout *innerLayout = itemInnerLayouts.at(i);
                for (int j = 0; j < innerLayout->count(); ++j)
                {
                    QLayoutItem *item = innerLayout->itemAt(j);
                    if (item && item->widget())
                    {
                        QSize itemSize = item->widget()->size();
                        if (itemSize.width() > colWidth) colWidth = itemSize.width();
                        colHeight += (10 + itemSize.height());
                    }
                    else colHeight += 10; // empty item, shouldn't happen
                }
                if (colHeight > newHeight) newHeight = colHeight;
                newWidth += (10 + colWidth);
            }
        }
        else
        {
            // horizontal, then vertical
            for (int i = 0; i < itemInnerLayouts.count(); ++i)
            {
                int colHeight = 0;
                int colWidth = 0;
                QBoxLayout *innerLayout = itemInnerLayouts.at(i);
                for (int j = 0; j < innerLayout->count(); ++j)
                {
                    QLayoutItem *item = innerLayout->itemAt(j);
                    if (item && item->widget())
                    {
                        QSize itemSize = item->widget()->size();
                        if (itemSize.height() > colHeight) colHeight = itemSize.height();
                        colWidth += (10 + itemSize.width());
                    }
                    else colWidth += 10; // empty item, shouldn't happen
                }
                if (colWidth > newWidth) newWidth = colWidth;
                newHeight += (10 + colHeight);
            }
        }
    }
    else
    {
        QSize fixedSize = setwin->GetFixedSize();
        newWidth = fixedSize.width();
        newHeight = fixedSize.height();
    }
    if (newWidth < (icon_size + 10)) newWidth = icon_size + 10;
    if (newWidth > screen_size.width()) newWidth = screen_size.width();
    if (newHeight < (icon_size + 10)) newHeight = icon_size + 10;
    if (newHeight > screen_size.height()) newHeight = screen_size.height();
    setFixedSize(newWidth, newHeight);
    int newX = setwin->IsFromLeft() ? 0 : screen_size.width() - newWidth;
    int newY = setwin->IsFromTop() ? 0 : screen_size.height() - newHeight;
    move(newX, newY);
}

void wmiib2::GenerateMask()
{
    QBitmap bitmap(size());
    if (setwin->IsTransparent())
    {
        ui->frame->setFrameShape(QFrame::NoFrame);
        bitmap.clear();
        QPainter painter(&bitmap);
        painter.setBrush(QBrush(Qt::color1));
        QMap<xcb_window_t, QLabel *>::iterator p;
        for (p = win_icon.begin(); p != win_icon.end(); ++p)
        {
            QPoint winpos = p.value()->mapTo(this, QPoint(0, 0));
            QRect winrect(winpos, p.value()->size());
            painter.fillRect(winrect.marginsAdded(QMargins(1, 3, 1, 1)), Qt::color1);
        }
        int btnX = setwin->IsFromLeft() ? 1 : width() - 9;
        int btnY = setwin->IsFromTop() ? 1 : height() - 9;
        painter.drawRoundRect(btnX, btnY, 8, 8, 2, 2);
    }
    else
    {
        ui->frame->setFrameShape(QFrame::Box);
        bitmap.fill(Qt::color1);
    }
    setMask(bitmap);
}

void wmiib2::SettingsChanged()
{
    // color
    MyPalette.setColor(QPalette::Base, setwin->GetBackgroundColor());
    MyPalette.setColor(QPalette::Window, setwin->GetBackgroundColor());
    setPalette(MyPalette);
    // fix the layout orientations
    itemOuterLayout->setDirection(setwin->GetOuterLayoutDirection());
    QBoxLayout::Direction direction = setwin->GetInnerLayoutDirection();
    for (int i = 0; i < itemInnerLayouts.count(); ++i)
    {
        itemInnerLayouts.at(i)->setDirection(direction);
        itemOuterLayout->setAlignment(itemInnerLayouts.at(i), setwin->GetIconAlignment());
    }
    // see if the icon size changed
    int icon_size = setwin->GetIconSize();
    if (icon_size != saved_icon_size)
    {
        // just take them all out of the layouts and rebuild
        QList<QLabel *> labels;
        // let's get them out in order
        for (int i = 0; i < itemInnerLayouts.count(); ++i)
        {
            QBoxLayout *itemLayout = itemInnerLayouts.at(i);
            while (itemLayout->count())
            {
                QLayoutItem *layoutItem = itemLayout->takeAt(0);
                if (layoutItem && layoutItem->widget())
                {
                    labels.append((QLabel *)(layoutItem->widget()));
                    delete layoutItem;
                }
            }
        }
        while (labels.count())
        {
            QLabel *label = labels.takeFirst();
            QMap<xcb_window_t, QLabel *>::iterator p;
            for (p = win_icon.begin(); p != win_icon.end(); ++p)
            {
                if (p.value() == label)
                {
                    // found -- update pixmap
                    QPixmap win_pm = win_info[p.key()]->GetPixmap().scaled(icon_size, icon_size, Qt::KeepAspectRatio);
                    label->setPixmap(win_pm);
                    label->setFixedSize(win_pm.size());
                    break;
                }
            }
            AddWidgetToLayout(label);
        }
        saved_icon_size = icon_size;
    }
    AdjustFrameSize();
    // force redraw?
    update();
}

void wmiib2::RemoveWindowIcon(xcb_window_t win)
{
    if (win_icon.contains(win))
    {
        if (QLabel *label = win_icon[win])
        {
            // find the layout that contains it
            int containingLayout = -1;
            QLayoutItem *containerItem = nullptr;
            for (int i = 0; i < itemInnerLayouts.count(); ++i)
            {
                QBoxLayout *innerLayout = itemInnerLayouts.at(i);
                for (int j = 0; j < innerLayout->count(); ++j)
                {
                    QLayoutItem *layoutItem = innerLayout->itemAt(j);
                    if (layoutItem && layoutItem->widget())
                    {
                        if (layoutItem->widget() == label)
                        {
                            containerItem = layoutItem;
                            containingLayout = i;
                            break;
                        }
                    }
                }
                if (containerItem) break;
            }
            if (containerItem)
            {
                itemInnerLayouts.at(containingLayout)->removeItem(containerItem);
                win_icon[win]->deleteLater();
                TryToShiftItemInLayout(containingLayout + 1);
            }
        }
        win_icon.remove(win);
        AdjustFrameSize();
    }


}

void wmiib2::AddWidgetToLayout(QLabel *newItem)
{
    // find a layout with enough room
    QSize newItemSize = newItem->size();
    QBoxLayout::Direction direction = setwin->GetInnerLayoutDirection();
    QSize screen_size = QGuiApplication::primaryScreen()->size();
    for (int i = 0; i < itemInnerLayouts.count(); ++i)
    {
        int currentSize = GetLayoutSize(i);
        if (direction == QBoxLayout::TopToBottom || direction == QBoxLayout::BottomToTop)
        {
            // vertical
            if ((currentSize + newItemSize.height() + 10) <= screen_size.height())
            {
                // append it here
                itemInnerLayouts.at(i)->addWidget(newItem);
                itemInnerLayouts.at(i)->setAlignment(newItem, setwin->GetIconAlignment());
                return;
            }
        }
        else
        {
            // horizontal
            if ((currentSize + newItemSize.width() + 10) <= screen_size.width())
            {
                // append it here
                itemInnerLayouts.at(i)->addWidget(newItem);
                itemInnerLayouts.at(i)->setAlignment(newItem, setwin->GetIconAlignment());
                return;
            }
        }
    }
    // time to add another layout
    QBoxLayout *innerLayout = new QBoxLayout(setwin->GetInnerLayoutDirection());
    innerLayout->setMargin(0);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(10);
    itemOuterLayout->addLayout(innerLayout);
    itemOuterLayout->setAlignment(innerLayout, setwin->GetIconAlignment());
    itemInnerLayouts.append(innerLayout);
    innerLayout->addWidget(newItem);
    innerLayout->setAlignment(newItem, setwin->GetIconAlignment());
}

void wmiib2::TryToShiftItemInLayout(int layoutIndex)
{
    if (layoutIndex < 1) return;
    // if we have multiple layouts, we may need to move one item
    if (itemInnerLayouts.count() > layoutIndex)
    {
        if (itemInnerLayouts.at(layoutIndex)->count())
        {
            // see if we have room for it
            int currentSize = GetLayoutSize(layoutIndex - 1);
            QBoxLayout::Direction direction = setwin->GetInnerLayoutDirection();
            QSize screen_size = QGuiApplication::primaryScreen()->size();
            // get the next icon size
            if (QLayoutItem *layoutItem = itemInnerLayouts.at(layoutIndex)->itemAt(0))
            {
                if (QWidget *itemWidget = layoutItem->widget())
                {
                    if (direction == QBoxLayout::TopToBottom || direction == QBoxLayout::BottomToTop)
                    {
                        // vertical
                        if ((currentSize + itemWidget->height() + 10) <= screen_size.height())
                        {
                            // shift item to previous sublayout
                            QLayoutItem *takenItem = itemInnerLayouts.at(layoutIndex)->takeAt(0);
                            itemInnerLayouts.at(layoutIndex - 1)->addItem(takenItem);
                            // try again with next index
                            TryToShiftItemInLayout(layoutIndex + 1);
                            return;
                        }
                    }
                    else
                    {
                        if ((currentSize + itemWidget->width() + 10) <= screen_size.width())
                        {
                            // shift item to previous sublayout
                            QLayoutItem *takenItem = itemInnerLayouts.at(layoutIndex)->takeAt(0);
                            itemInnerLayouts.at(layoutIndex - 1)->addItem(takenItem);
                            // try again with next index
                            TryToShiftItemInLayout(layoutIndex + 1);
                            return;
                        }
                    }
                }
            }
        }
    }
}

int wmiib2::GetLayoutSize(int layoutIndex)
{
    QBoxLayout *innerLayout = itemInnerLayouts.at(layoutIndex);
    int currentSize = 10; // margins
    QBoxLayout::Direction direction = setwin->GetInnerLayoutDirection();
    for (int i = 0; i < innerLayout->count(); ++i)
    {
        if (QLayoutItem *layoutItem = innerLayout->itemAt(i))
        {
            currentSize += 10;
            if (QWidget *itemWidget = layoutItem->widget())
            {
                if (direction == QBoxLayout::TopToBottom || direction == QBoxLayout::BottomToTop)
                {
                    // vertical
                    currentSize += itemWidget->height();
                }
                else
                {
                    // horizontal
                    currentSize += itemWidget->width();
                }
            }
        }
    }
    return currentSize;
}

