#pragma once

#include "core/types.hpp"
#include <cstdint>

namespace cscpp::assets::bsp {

// GoldSrc BSP file format structures
// Based on Valve's Half-Life BSP format

struct BSPLump {
    i32 offset;
    i32 length;
};

struct BSPHeader {
    i32 version;        // BSP version (30 for GoldSrc)
    BSPLump lumps[15];  // Lump directory
};

// Lump indices
enum BSPLumpType {
    LUMP_ENTITIES = 0,
    LUMP_PLANES = 1,
    LUMP_TEXTURES = 2,
    LUMP_VERTICES = 3,
    LUMP_VISIBILITY = 4,
    LUMP_NODES = 5,
    LUMP_TEXINFO = 6,
    LUMP_FACES = 7,
    LUMP_LIGHTING = 8,
    LUMP_CLIPNODES = 9,
    LUMP_LEAVES = 10,
    LUMP_MARKSURFACES = 11,
    LUMP_EDGES = 12,
    LUMP_SURFEDGES = 13,
    LUMP_MODELS = 14
};

struct BSPPlane {
    f32 normal[3];
    f32 distance;
    i32 type;
};

struct BSPVertex {
    f32 position[3];
};

struct BSPEdge {
    u16 vertexIndices[2];
};

struct BSPFace {
    u16 planeIndex;
    u16 side;
    i32 firstEdge;
    u16 numEdges;
    u16 textureInfo;
    u8 lightmapStyles[4];
    i32 lightmapOffset;
};

struct BSPTextureInfo {
    f32 vecs[2][4];  // Texture vectors
    i32 miptex;
    i32 flags;
};

struct BSPModel {
    f32 mins[3];
    f32 maxs[3];
    f32 origin[3];
    i32 headNodes[4];
    i32 visLeafs;
    i32 firstFace;
    i32 numFaces;
};

// Miptex structure (texture format in BSP and WAD)
struct BSPMiptex {
    char name[16];      // Texture name
    u32 width;          // Texture width
    u32 height;         // Texture height
    u32 offsets[4];     // Offsets to mip levels (0=full, 1=half, 2=quarter, 3=eighth)
};

// WAD file format structures (GoldSrc WAD3 format)
struct WADHeader {
    char magic[4];      // "WAD3" for GoldSrc
    i32 numEntries;    // Number of directory entries
    i32 dirOffset;     // Offset to directory
};

struct WADEntry {
    i32 offset;        // Offset to texture data
    i32 diskSize;      // Size on disk (compressed)
    i32 size;          // Uncompressed size
    u8 type;           // Entry type (0x43 = miptex)
    u8 compression;    // Compression type
    u16 dummy;         // Padding
    char name[16];     // Texture name (null-terminated)
};

} // namespace cscpp::assets::bsp

