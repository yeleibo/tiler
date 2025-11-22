#include "TileMap.h"
#include "Utils.h"

TileMap::TileMap()
    : m_id(0), m_minZoom(0), m_maxZoom(20), m_schema("xyz")
{
}

QString TileMap::getTileUrl(const Tile& tile) const
{
    return getTileUrl(tile.z, tile.x, tile.y);
}

QString TileMap::getTileUrl(int z, int x, int y) const
{
    return Utils::replaceTileUrl(m_url, z, x, y);
}
