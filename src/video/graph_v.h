#ifndef GRAPH_V_H
#define GRAPH_V_H

#include "null_v.h"

class VideoDriver_Graph : public VideoDriver_Null
{
public:
	VideoDriver_Graph();
	/* virtual */ void MainLoop();

	/* virtual */ const char *GetName() const { return "graph"; }
};

/** Factory the graph video driver. */
class FVideoDriver_Graph : public DriverFactoryBase {
public:
	FVideoDriver_Graph() : DriverFactoryBase(Driver::DT_VIDEO, 0, "graph", "Graph Video Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new VideoDriver_Graph(); }
};

#endif // GRAPH_V_H
