/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
 *
 *****************************************************************************/

/**
   \file
   EtherCAT Slave Database.
*/

/*****************************************************************************/

#ifndef __ECDB_H__
#define __ECDB_H__

/*****************************************************************************/

/** \cond */

#define Beckhoff_EL1004_Inputs 0x00000002, 0x03EC3052, 0x3101, 1

#define Beckhoff_EL1014_Inputs 0x00000002, 0x03F63052, 0x3101, 1

#define Beckhoff_EL2004_Outputs 0x00000002, 0x07D43052, 0x3001, 1

#define Beckhoff_EL2032_Outputs 0x00000002, 0x07F03052, 0x3001, 1

#define Beckhoff_EL3102_Status1 0x00000002, 0x0C1E3052, 0x3101, 1
#define Beckhoff_EL3102_Input1 0x00000002, 0x0C1E3052, 0x3101, 2
#define Beckhoff_EL3102_Status2 0x00000002, 0x0C1E3052, 0x3102, 1
#define Beckhoff_EL3102_Input2 0x00000002, 0x0C1E3052, 0x3102, 2

#define Beckhoff_EL3152_Status1 0x00000002, 0x0C503052, 0x3101, 1
#define Beckhoff_EL3152_Input1 0x00000002, 0x0C503052, 0x3101, 2
#define Beckhoff_EL3152_Status2 0x00000002, 0x0C503052, 0x3102, 1
#define Beckhoff_EL3152_Input2 0x00000002, 0x0C503052, 0x3102, 2

#define Beckhoff_EL3162_Status1 0x00000002, 0x0C5A3052, 0x3101, 1
#define Beckhoff_EL3162_Input1 0x00000002, 0x0C5A3052, 0x3101, 2
#define Beckhoff_EL3162_Status2 0x00000002, 0x0C5A3052, 0x3102, 1
#define Beckhoff_EL3162_Input2 0x00000002, 0x0C5A3052, 0x3102, 2

#define Beckhoff_EL4102_Output1 0x00000002, 0x10063052, 0x6411, 1
#define Beckhoff_EL4102_Output2 0x00000002, 0x10063052, 0x6411, 2

#define Beckhoff_EL4132_Output1 0x00000002, 0x10243052, 0x6411, 1
#define Beckhoff_EL4132_Output2 0x00000002, 0x10243052, 0x6411, 2

#define Beckhoff_EL5001_Status 0x00000002, 0x13893052, 0x3101, 1
#define Beckhoff_EL5001_Value  0x00000002, 0x13893052, 0x3101, 2

#define Beckhoff_EL5101_Value 0x00000002, 0x13ED3052, 0x6000, 2

/** \endcond */

/*****************************************************************************/

#endif
