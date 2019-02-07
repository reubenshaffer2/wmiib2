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
#include "settingswindow.h"
#include "ui_settingswindow.h"
#include <QSettings>
#include <QGuiApplication>
#include <QScreen>
#include <QColorDialog>

SettingsWindow::SettingsWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingsWindow)
{
    ui->setupUi(this);
    bgColorPal = new QPalette(palette());
    ui->BGColorLabel->setBackgroundRole(QPalette::Base);
    store = new QSettings("bobshaffer.net", "wmiib2", this);
    store->beginGroup("iconbox");
    QString ib_size_type = store->value("size_type", QVariant("")).toString();
    if (ib_size_type == "growing")
    {
        ui->ST_Growing->setChecked(true);
    }
    else
    {
        if (ib_size_type != "fixed")
        {
            ib_size_type = "fixed";
            store->setValue("size_type", QVariant("fixed"));
        }
        ui->ST_Fixed->setChecked(true);
    }
    // set some arbitrary limits on icon size
    int ib_icon_size = store->value("icon_size", QVariant(0)).toInt();
    if (ib_icon_size < 20 || ib_icon_size > 300)
    {
        if (ib_icon_size < 20) ib_icon_size = 20;
        if (ib_icon_size > 300) ib_icon_size = 300;
        store->setValue("icon_size", QVariant(ib_icon_size));
    }
    ui->IconSize->setMinimum(20);
    ui->IconSize->setMaximum(300);
    ui->IconSize->setValue(ib_icon_size);
    QString ib_bg_transparent = store->value("background_transparent", QVariant("")).toString();
    if (ib_bg_transparent != "true" && ib_bg_transparent != "false")
    {
        ib_bg_transparent = "false";
        store->setValue("background_transparent", QVariant("false"));
    }
    ui->TransparentBackground->setChecked(ib_bg_transparent == "true");
    // ui->BGColorLabel->setText(checked ? "Border Color" : "Background Color");
    QString ib_location = store->value("location", QVariant("")).toString();
    if (ib_location == "bottom_right")
    {
        ui->Location->setCurrentText("Bottom Right");
    }
    else if (ib_location == "bottom_left")
    {
        ui->Location->setCurrentText("Bottom Left");
    }
    else if (ib_location == "top_right")
    {
        ui->Location->setCurrentText("Top Right");
    }
    else if (ib_location == "top_left")
    {
        ui->Location->setCurrentText("Top Left");
    }
    else
    {
        // default to bottom right
        store->setValue("location", QVariant("bottom_right"));
        ui->Location->setCurrentText("Bottom Right");
    }
    if (ib_size_type == "fixed")
    {
        // width
        int ib_width = store->value("fixed_width", QVariant(0)).toInt();
        if (ib_width < (ib_icon_size + 10))
        {
            ib_width = ib_icon_size + 10;
            store->setValue("fixed_width", QVariant(ib_width));
        }
        if (ib_width > QGuiApplication::primaryScreen()->size().width())
        {
            ib_width = QGuiApplication::primaryScreen()->size().width();
            store->setValue("fixed_width", QVariant(ib_width));
        }
        ui->FixedWidth->setMinimum(ib_icon_size + 10);
        ui->FixedWidth->setMaximum(QGuiApplication::primaryScreen()->size().width());
        ui->FixedWidth->setValue(ib_width);
        // height
        int ib_height = store->value("fixed_height", QVariant(0)).toInt();
        if (ib_height < (ib_icon_size + 10))
        {
            ib_height = ib_icon_size + 10;
            store->setValue("fixed_height", QVariant(ib_height));
        }
        if (ib_height > QGuiApplication::primaryScreen()->size().height())
        {
            ib_height = QGuiApplication::primaryScreen()->size().height();
            store->setValue("fixed_height", QVariant(ib_height));
        }
        ui->FixedHeight->setMinimum(ib_icon_size + 10);
        ui->FixedHeight->setMaximum(QGuiApplication::primaryScreen()->size().height());
        ui->FixedHeight->setValue(ib_height);
    }
    // grow direction
    QString ib_growdir = store->value("grow_direction", QVariant("")).toString();
    if (ib_growdir == "horizontal")
    {
        ui->GrowDirection->setCurrentText("Horizontal");
    }
    else if (ib_growdir == "vertical")
    {
        ui->GrowDirection->setCurrentText("Vertical");
    }
    else
    {
        // default to horizontal
        ib_growdir = "horizontal";
        ui->GrowDirection->setCurrentText("Horizontal");
        store->setValue("grow_direction", QVariant(ib_growdir));
    }
    // background color - we will default to the system default
    QColor ib_bgcolor = store->value("background_color", QVariant(bgColorPal->color(QPalette::Base))).value<QColor>();
    if (ib_bgcolor != bgColorPal->color(QPalette::Base))
    {
        bgColorPal->setColor(QPalette::Base, ib_bgcolor);
    }
    ui->BGColorLabel->setPalette(*bgColorPal);
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
    delete bgColorPal;
}

QBoxLayout::Direction SettingsWindow::GetOuterLayoutDirection() const
{
    QString grow_direction = ui->GrowDirection->currentText();
    QString ib_location = ui->Location->currentText();
    QStringList origin = ib_location.split(' ');
    if (grow_direction == "Horizontal")
    {
        if (origin.at(0) == "Top")
            return QBoxLayout::TopToBottom;
        else // if (origin.at(0) == "Bottom")
            return QBoxLayout::BottomToTop;
    }
    else // if (grow_direction == "Vertical")
    {
        if (origin.at(1) == "Left")
            return QBoxLayout::LeftToRight;
        else // if (origin.at(1) == "Right")
            return QBoxLayout::RightToLeft;
    }
}

QBoxLayout::Direction SettingsWindow::GetInnerLayoutDirection() const
{
    QString grow_direction = ui->GrowDirection->currentText();
    QString ib_location = ui->Location->currentText();
    QStringList origin = ib_location.split(' ');
    if (grow_direction == "Vertical")
    {
        if (origin.at(0) == "Top")
            return QBoxLayout::TopToBottom;
        else // if (origin.at(0) == "Bottom")
            return QBoxLayout::BottomToTop;
    }
    else // if (grow_direction == "Horizontal")
    {
        if (origin.at(1) == "Left")
            return QBoxLayout::LeftToRight;
        else // if (origin.at(1) == "Right")
            return QBoxLayout::RightToLeft;
    }
}

Qt::Alignment SettingsWindow::GetIconAlignment() const
{
    QString grow_direction = ui->GrowDirection->currentText();
    QString ib_location = ui->Location->currentText();
    QStringList origin = ib_location.split(' ');
    if (grow_direction == "Vertical")
    {
        if (origin.at(0) == "Top")
            return Qt::AlignTop;
        else // if (origin.at(0) == "Bottom")
            return Qt::AlignBottom;
    }
    else // if (grow_direction == "Horizontal")
    {
        if (origin.at(1) == "Left")
            return Qt::AlignLeft;
        else // if (origin.at(1) == "Right")
            return Qt::AlignRight;
    }
}

int SettingsWindow::GetIconSize() const
{
    return ui->IconSize->value();
}

QSize SettingsWindow::GetFixedSize() const
{
    if (ui->ST_Fixed->isChecked())
        return QSize(ui->FixedWidth->value(), ui->FixedHeight->value());
    else return QSize();
}

bool SettingsWindow::DoesGrow() const
{
    return (ui->ST_Growing->isChecked());
}

bool SettingsWindow::IsFromTop() const
{
    QString ib_location = ui->Location->currentText();
    QStringList origin = ib_location.split(' ');
    return (origin.at(0) == "Top");
}

bool SettingsWindow::IsFromLeft() const
{
    QString ib_location = ui->Location->currentText();
    QStringList origin = ib_location.split(' ');
    return (origin.at(1) == "Left");
}

bool SettingsWindow::IsTransparent() const
{
    return ui->TransparentBackground->isChecked();
}

QColor SettingsWindow::GetBackgroundColor() const
{
    return bgColorPal->color(QPalette::Base);
}

void SettingsWindow::changeEvent(QEvent *e)
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

void SettingsWindow::hideEvent(QHideEvent *e)
{
    emit settingsChanged();
    QWidget::hideEvent(e);
}

void SettingsWindow::closeEvent(QCloseEvent *e)
{
    emit settingsChanged();
    QWidget::closeEvent(e);
}

void SettingsWindow::on_tabWidget_currentChanged(int index)
{
    if (index == ui->tabWidget->indexOf(ui->ST_Fixed_Tab)) ui->ST_Fixed->setChecked(true);
    else if (index == ui->tabWidget->indexOf(ui->ST_Growing_Tab)) ui->ST_Growing->setChecked(true);
}

void SettingsWindow::on_ST_Fixed_toggled(bool checked)
{
    if (checked)
    {
        int ib_icon_size = ui->IconSize->value();
        store->setValue("size_type", QVariant("fixed"));
        ui->tabWidget->setCurrentWidget(ui->ST_Fixed_Tab);
        // width
        int ib_width = store->value("fixed_width", QVariant(0)).toInt();
        if (ib_width < (ib_icon_size + 10))
        {
            ib_width = ib_icon_size + 10;
            store->setValue("fixed_width", QVariant(ib_width));
        }
        if (ib_width > QGuiApplication::primaryScreen()->size().width())
        {
            ib_width = QGuiApplication::primaryScreen()->size().width();
            store->setValue("fixed_width", QVariant(ib_width));
        }
        ui->FixedWidth->setMinimum(ib_icon_size + 10);
        ui->FixedWidth->setMaximum(QGuiApplication::primaryScreen()->size().width());
        ui->FixedWidth->setValue(ib_width);
        // height
        int ib_height = store->value("fixed_height", QVariant(0)).toInt();
        if (ib_height < (ib_icon_size + 10))
        {
            ib_height = ib_icon_size + 10;
            store->setValue("fixed_height", QVariant(ib_height));
        }
        if (ib_height > QGuiApplication::primaryScreen()->size().height())
        {
            ib_height = QGuiApplication::primaryScreen()->size().height();
            store->setValue("fixed_height", QVariant(ib_height));
        }
        ui->FixedHeight->setMinimum(ib_icon_size + 10);
        ui->FixedHeight->setMaximum(QGuiApplication::primaryScreen()->size().height());
        ui->FixedHeight->setValue(ib_height);
    }
}

void SettingsWindow::on_ST_Growing_toggled(bool checked)
{
    if (checked)
    {
        store->setValue("size_type", QVariant("growing"));
        ui->tabWidget->setCurrentWidget(ui->ST_Growing_Tab);
    }
}

void SettingsWindow::on_Location_activated(const QString &arg1)
{
    store->setValue("location", QVariant(arg1.toLower().replace(' ', "_")));
}

void SettingsWindow::on_IconSize_valueChanged(int arg1)
{
    store->setValue("icon_size", QVariant(arg1));
}

void SettingsWindow::on_TransparentBackground_toggled(bool checked)
{
    store->setValue("background_transparent", QVariant(checked ? "true" : "false"));
    ui->BGColorLabel->setText(checked ? "Border Color" : "Background Color");
}

void SettingsWindow::on_FixedWidth_valueChanged(int arg1)
{
    store->setValue("fixed_width", QVariant(arg1));
}

void SettingsWindow::on_FixedHeight_valueChanged(int arg1)
{
    store->setValue("fixed_height", QVariant(arg1));
}

void SettingsWindow::on_GrowDirection_activated(const QString &arg1)
{
    store->setValue("grow_direction", QVariant(arg1.toLower()));
}

void SettingsWindow::on_BGColorButton_clicked()
{
    QColor newColor = QColorDialog::getColor(bgColorPal->color(QPalette::Base), this, QString("Select Color"));
    if (newColor.isValid())
    {
        store->setValue("background_color", QVariant(newColor));
        bgColorPal->setColor(QPalette::Base, newColor);
        ui->BGColorLabel->setPalette(*bgColorPal);
    }
}
