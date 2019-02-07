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
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    wmiib2 w;
    w.show();

    return a.exec();
}
