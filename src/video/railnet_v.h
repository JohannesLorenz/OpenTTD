/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file railnet_v.h declaration of VideoDriver_Railnet */

#if __cplusplus >= 201103L || defined(IDE_USED)
#ifndef RAILNET_V_H
#define RAILNET_V_H

#include "null_v.h"
#include <vector>

typedef uint32 CargoLabel; // forward declaration

namespace comm {
	struct RailnetFileInfo;
}

/**
 * Video driver that does not blit. Instead, it outputs a
 * railnet json file.
 */
class VideoDriver_Railnet : public VideoDriver_Null {
	void SaveOrderList(comm::RailnetFileInfo& file, const Train *train,
		std::vector<bool> &stations_used, std::set<CargoLabel> &cargo_used, std::set<const OrderList *> &order_lists_done,
		class NodeListT& node_list) const;
	void SaveStation(comm::RailnetFileInfo& file, const struct BaseStation* st,
		const std::vector<bool> &stations_used) const;
	void SaveCargoLabels(comm::RailnetFileInfo &file, std::set<CargoLabel> &s) const;
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

#endif // RAILNET_V_H
#endif

