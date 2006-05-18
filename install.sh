#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT install script
#
#  $Id$
#
#  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  The IgH EtherCAT Master is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the IgH EtherCAT Master; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#  The right to use EtherCAT Technology is granted and comes free of
#  charge under condition of compatibility of product made by
#  Licensee. People intending to distribute/sell products based on the
#  code, have to sign an agreement to guarantee that products using
#  software based on IgH EtherCAT master stay compatible with the actual
#  EtherCAT specification (which are released themselves as an open
#  standard) as the (only) precondition to have the right to use EtherCAT
#  Technology, IP and trade marks.
#
#------------------------------------------------------------------------------

CONFIGFILE=/etc/sysconfig/ethercat

#------------------------------------------------------------------------------

# install function

install()
{
    echo "    $1"
    if ! cp $1 $INSTALLDIR; then exit 1; fi
}

#------------------------------------------------------------------------------

# Fetch parameter

if [ $# -ne 2 ]; then
    echo "This script is called by \"make\". Run \"make install\" instead."
    exit 1
fi

KERNEL=$1
DEVICEINDEX=$2

INSTALLDIR=/lib/modules/$KERNEL/kernel/drivers/net
echo "EtherCAT installer - Kernel: $KERNEL"

# Copy files

echo "  installing modules..."
install master/ec_master.ko
install devices/ec_8139too.ko

# Update dependencies

echo "  building module dependencies..."
depmod

# Create configuration file

if [ -f $CONFIGFILE ]; then
    echo "  notice: using existing configuration file."
else
    echo "  creating $CONFIGFILE..."
    echo "DEVICEINDEX=$DEVICEINDEX" > $CONFIGFILE || exit 1
fi

# Install rc script

echo "  installing startup script..."
cp ethercat.sh /etc/init.d/ethercat || exit 1
chmod +x /etc/init.d/ethercat || exit 1
if [ ! -L /usr/sbin/rcethercat ]; then
    ln -s /etc/init.d/ethercat /usr/sbin/rcethercat || exit 1
fi

# Install tools

echo "  installing tools..."
cp tools/ec_list.pl /usr/local/bin/ec_list || exit 1
chmod +x /usr/local/bin/ec_list || exit 1

# Finish

echo "EtherCAT installer done."
exit 0

#------------------------------------------------------------------------------
