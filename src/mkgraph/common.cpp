#ifdef EXPORTER
#include <cassert>
#else
#include "../stdafx.h"
#endif

#include <iostream>
#include "common.h"

const std::string railnet_file_info::hdr = "railnet";
const uint railnet_file_info::version = 0;

void _wrt(std::ostream& o, const char* raw, std::size_t sz) {
	o.write(raw, sz);
}

void _rd(std::istream& i, char* raw, std::size_t sz) {
	i.read(raw, sz);
}

bool order_list::operator<(const order_list &other) const
{
	return (min_station == other.min_station)
		? stations.size() < other.stations.size()
		: min_station < other.min_station;
}

void serialize(const order_list &ol, std::ostream &o)
{
	serialize(ol.is_cycle, o);
	serialize(ol.is_bicycle, o);
	serialize(ol.min_station, o);
	serialize(ol.cargo, o);
	serialize(ol.stations, o);
}

void deserialize(order_list &ol, std::istream &i)
{
	deserialize(ol.is_cycle, i);
	deserialize(ol.is_bicycle, i);
	deserialize(ol.min_station, i);
	deserialize(ol.cargo, i);
	deserialize(ol.stations, i);
}



void serialize(const station_info &si, std::ostream &o)
{
	serialize(si.name, o);
	serialize(si.x, o);
	serialize(si.y, o);
}


void deserialize(station_info &si, std::istream &i)
{
	deserialize(si.name, i);
	deserialize(si.x, i);
	deserialize(si.y, i);
}

void serialize(const railnet_file_info &file, std::ostream &o)
{
	serialize(file.hdr, o);
	serialize(file.version, o);
	serialize(file.order_lists, o);
	serialize(file.stations, o);
	serialize(file.cargo_names, o);
}

void deserialize(railnet_file_info &file, std::istream &i)
{
	std::string found_hdr;
	deserialize(found_hdr, i);
	assert(found_hdr==file.hdr);
	uint version;
	deserialize(version, i);
	assert(version==file.version);
	deserialize(file.order_lists, i);
	deserialize(file.stations, i);
	deserialize(file.cargo_names, i);
}

