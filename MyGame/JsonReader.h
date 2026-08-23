#pragma once

#include "json.hpp"
#include <iostream>
#include <fstream>
#include <string>


using json = nlohmann::json;

struct MapInfo
{
	int width, height, tilewidth, tileheight;
	std::vector<int> data;

	static MapInfo GetFromJSON(json &json) {

		return MapInfo{
				json["width"].get<int>(),
				json["height"].get<int>(),
				json["tilewidth"].get<int>(),
				json["tileheight"].get<int>(),
				json["data"].get<std::vector<int>>(),
		};

	}

	static MapInfo GetFromJSON(json &json, MapInfo& mapInfo) {

		mapInfo.width = json["width"].get<int>();
		mapInfo.height = json["height"].get<int>();
		mapInfo.tilewidth = json["tilewidth"].get<int>();
		mapInfo.tileheight = json["tileheight"].get<int>();
		mapInfo.data = json["data"].get<std::vector<int>>();

		return mapInfo;
	}

	static MapInfo GetFromFile(const char* path) {
		std::ifstream i(path);
		json json;
		i >> json;
		i.close();

		return MapInfo::GetFromJSON(json);
	}

	static void GetFromFile(const char* path, MapInfo &mapInfo) {
		std::ifstream i(path);
		json json;
		i >> json;
		i.close();

		MapInfo::GetFromJSON(json, mapInfo);
	}
};

