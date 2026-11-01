#ifndef MAP_DATA_H
#define MAP_DATA_H
struct GeoPoint { double x, y; };
struct MapRing { const GeoPoint* points; int count; int region; };
struct RegionInfo {
    const wchar_t* name;
    const wchar_t* shortName;
    const char* iso;
    double labelX, labelY;
};
extern const MapRing MapRings[];
extern const int MapRingCount;
extern const RegionInfo Regions[27];
extern const double MapMinX, MapMinY, MapMaxX, MapMaxY;
#endif
