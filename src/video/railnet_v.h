#ifndef GRAPH_V_H
#define GRAPH_V_H

#include "null_v.h"
#include <vector>

typedef uint32 CargoLabel; // forward declaration

class VideoDriver_Railnet : public VideoDriver_Null
{
	void SaveOrderList(struct railnet_file_info& file, const Train *train,
		std::vector<bool> &stations_used, std::set<CargoLabel> &cargo_used, std::set<const OrderList *> &order_lists_done) const;
	void SaveStation(struct railnet_file_info& file, const struct BaseStation* st,
		const std::vector<bool> &stations_used) const;
	void SaveCargoLabels(railnet_file_info &file, std::set<CargoLabel> &s) const;
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
