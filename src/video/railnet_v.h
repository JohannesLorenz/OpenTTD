/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRAPH_V_H
#define GRAPH_V_H

#include "null_v.h"
#include <vector>

typedef uint32 CargoLabel; // forward declaration

namespace comm {
	struct railnet_file_info;
}

class VideoDriver_Railnet : public VideoDriver_Null {
	void SaveOrderList(comm::railnet_file_info& file, const Train *train,
		std::vector<bool> &stations_used, std::set<CargoLabel> &cargo_used, std::set<const OrderList *> &order_lists_done,
		struct node_list_t& node_list) const;
	void SaveStation(comm::railnet_file_info& file, const struct BaseStation* st,
		const std::vector<bool> &stations_used) const;
	void SaveCargoLabels(comm::railnet_file_info &file, std::set<CargoLabel> &s) const;
public:
	VideoDriver_Railnet();
	/* virtual */ void MainLoop();

	/* virtual */ const char *GetName() const { return "railnet"; }
};

/** Factory the railnet video driver. */
class FVideoDriver_Railnet : public DriverFactoryBase {
public:
	FVideoDriver_Railnet() : DriverFactoryBase(Driver::DT_VIDEO, 0, "railnet", "Railnet Video Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new VideoDriver_Railnet(); }
};

#endif // GRAPH_V_H
