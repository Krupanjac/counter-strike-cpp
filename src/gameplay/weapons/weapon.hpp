#pragma once

/**
 * @file weapon.hpp
 * @brief Weapon system definitions
 */

#include "core/types.hpp"
#include "core/math/math.hpp"

namespace cscpp::gameplay {

// ============================================================================
// Weapon IDs
// ============================================================================

enum class WeaponId : u8 {
    None = 0,
    
    // Pistols
    Glock18 = 1,
    USP = 2,
    P228 = 3,
    Deagle = 4,
    FiveSeven = 5,
    DualElites = 6,
    
    // SMGs
    MP5 = 10,
    TMP = 11,
    P90 = 12,
    Mac10 = 13,
    UMP45 = 14,
    
    // Rifles
    AK47 = 20,
    M4A1 = 21,
    SG552 = 22,
    AUG = 23,
    Famas = 24,
    Galil = 25,
    
    // Snipers
    AWP = 30,
    Scout = 31,
    SG550 = 32,
    G3SG1 = 33,
    
    // Shotguns
    M3 = 40,
    XM1014 = 41,
    
    // Machine Guns
    M249 = 50,
    
    // Equipment
    Knife = 60,
    HEGrenade = 61,
    Flashbang = 62,
    SmokeGrenade = 63,
    C4 = 64,
    
    Count
};

// ============================================================================
// Weapon Data
// ============================================================================

/**
 * @brief Static weapon data (from game data files)
 */
struct WeaponData {
    WeaponId id;
    const char* name;
    const char* viewModel;
    const char* worldModel;
    
    // Stats
    i32 damage;
    f32 armorPenetration;  // 0-1
    f32 range;
    f32 rangeModifier;     // Damage falloff
    
    // Fire rate
    f32 cycleTime;         // Time between shots
    bool isAutomatic;
    i32 burstCount;        // For burst fire weapons
    
    // Accuracy
    f32 baseAccuracy;
    f32 accuracyDivisor;
    f32 maxInaccuracy;
    f32 inaccuracyDecay;   // How fast accuracy recovers
    
    // Recoil
    f32 recoilMagnitude;
    f32 recoilAngle;       // Degrees from vertical
    
    // Spread
    f32 spreadBase;
    f32 spreadMove;        // Added when moving
    f32 spreadJump;        // Added when jumping
    f32 spreadDuck;        // Multiplier when ducking (usually < 1)
    
    // Ammo
    i32 clipSize;
    i32 maxAmmo;
    f32 reloadTime;
    
    // Movement
    f32 moveSpeedModifier; // Speed when holding this weapon
    
    // Cost
    i32 price;
    i32 killAward;
    
    // Flags
    bool canScope;
    f32 scopeFOV;          // FOV when scoped
    bool silenceable;
};

/**
 * @brief Get weapon data by ID
 */
const WeaponData& getWeaponData(WeaponId id);

// ============================================================================
// Recoil Pattern
// ============================================================================

/**
 * @brief Recoil pattern point
 */
struct RecoilPoint {
    f32 x;  // Horizontal offset
    f32 y;  // Vertical offset
};

/**
 * @brief Get recoil pattern for weapon
 */
const std::vector<RecoilPoint>& getRecoilPattern(WeaponId id);

// ============================================================================
// Weapon Logic
// ============================================================================

/**
 * @brief Calculate weapon spread
 */
struct SpreadParams {
    WeaponId weapon;
    bool isMoving;
    bool isJumping;
    bool isDucking;
    bool isScoped;
    i32 shotsFired;
    f32 lastShotTime;
    f32 currentTime;
};

Vec2 calculateSpread(const SpreadParams& params, u32 randomSeed);

/**
 * @brief Calculate recoil offset
 */
Vec2 calculateRecoil(WeaponId weapon, i32 shotsFired, u32 randomSeed);

/**
 * @brief Hitscan trace result
 */
struct HitscanResult {
    bool hit;
    Vec3 hitPosition;
    Vec3 hitNormal;
    entt::entity hitEntity;
    i32 hitGroup;  // Head, chest, etc.
    f32 damage;
};

/**
 * @brief Perform hitscan for weapon fire
 */
HitscanResult performHitscan(
    Vec3 origin,
    Vec3 direction,
    f32 range,
    entt::entity shooter,
    entt::registry& registry
);

// ============================================================================
// Hit Groups
// ============================================================================

enum class HitGroup : i32 {
    Generic = 0,
    Head = 1,
    Chest = 2,
    Stomach = 3,
    LeftArm = 4,
    RightArm = 5,
    LeftLeg = 6,
    RightLeg = 7,
};

/**
 * @brief Get damage multiplier for hit group
 */
f32 getHitGroupDamageMultiplier(HitGroup group);

} // namespace cscpp::gameplay

