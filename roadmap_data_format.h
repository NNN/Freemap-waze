/* roadmap_data_format.h - general format of tables in the RoadMap data files.
 *
 * LICENSE:
 *
 *   Copyright 2008 Israel Disatnik
 *
 *   This file is part of RoadMap.
 *
 *   RoadMap is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   RoadMap is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with RoadMap; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _ROADMAP_DATA_FORMAT__H_
#define _ROADMAP_DATA_FORMAT__H_

#define ROADMAP_DATA_TYPE   		".wdf"
#define ROADMAP_DATA_SIGNATURE	"WZDF"

#define ROADMAP_DATA_ENDIAN_CORRECT		0x00000001
#define ROADMAP_DATA_ENDIAN_REVERSED	0x01000000

#define ROADMAP_DATA_CURRENT_VERSION	0x00020000

typedef struct {

	char				signature[4];
	unsigned int	endianness;
	unsigned int	version;
	unsigned int	num_sections;
	unsigned int	byte_alignment_bits;	
} roadmap_data_header;

typedef struct {

	unsigned int	end_offset;
} roadmap_data_entry;
#endif // _ROADMAP_DATA_FORMAT__H_

