#pragma once

#include "core/types.hpp"
#include "core/platform/input.hpp"
#include <entt/entt.hpp>
#include <deque>

namespace cscpp::ecs {

/**
 * @brief Team enumeration
 */
enum class Team : u8 {
    Unassigned = 0,
    Spectator = 1,
    Terrorist = 2,
    CounterTerrorist = 3
};

/**
 * @brief Player component
 * 
 * Core player identification and state.
 */
struct PlayerComponent {
    ClientId clientId = INVALID_CLIENT_ID;
    std::string name;
    Team team = Team::Unassigned;
    bool isAlive = true;
    bool isBot = false;
    
    i32 kills = 0;
    i32 deaths = 0;
    i32 assists = 0;
    i32 score = 0;
    i32 money = 800;
};

/**
 * @brief Health and armor component
 */
struct HealthComponent {
    f32 health = 100.0f;
    f32 maxHealth = 100.0f;
    f32 armor = 0.0f;
    f32 maxArmor = 100.0f;
    
    enum class ArmorType : u8 {
        None = 0,
        Kevlar = 1,
        KevlarHelmet = 2
    };
    ArmorType armorType = ArmorType::None;
    
    /// Check if player is dead
    bool isDead() const { return health <= 0.0f; }
    
    /// Apply damage with armor reduction
    f32 applyDamage(f32 damage, bool isHeadshot = false) {
        if (armor > 0.0f && armorType != ArmorType::None) {
            // Headshot bypasses armor without helmet
            if (isHeadshot && armorType != ArmorType::KevlarHelmet) {
                health -= damage;
            } else {
                // Armor absorbs portion of damage
                f32 armorRatio = 0.5f;  // 50% armor protection
                f32 armorDamage = damage * armorRatio;
                
                if (armorDamage > armor) {
                    armorDamage = armor;
                }
                
                armor -= armorDamage;
                health -= damage - armorDamage;
            }
        } else {
            health -= damage;
        }
        
        if (health < 0.0f) health = 0.0f;
        return health;
    }
};

/**
 * @brief Input command buffer
 */
struct InputComponent {
    std::deque<UserCmd> pendingCmds;
    UserCmd latestCmd;
    Tick lastProcessedTick = 0;
    
    static constexpr size_t MAX_PENDING_CMDS = 128;
    
    void addCmd(const UserCmd& cmd) {
        pendingCmds.push_back(cmd);
        while (pendingCmds.size() > MAX_PENDING_CMDS) {
            pendingCmds.pop_front();
        }
        latestCmd = cmd;
    }
    
    UserCmd* getCmd(Tick tick) {
        for (auto& cmd : pendingCmds) {
            if (cmd.tick == tick) {
                return &cmd;
            }
        }
        return nullptr;
    }
};

/**
 * @brief Weapon inventory component
 */
struct InventoryComponent {
    static constexpr i32 MAX_WEAPONS = 5;
    
    struct WeaponSlot {
        u8 weaponId = 0;        // 0 = empty
        i32 ammo = 0;           // Current magazine ammo
        i32 reserveAmmo = 0;    // Reserve ammo
    };
    
    WeaponSlot slots[MAX_WEAPONS];
    u8 activeSlot = 0;          // Currently selected slot
    
    // Grenades
    i32 heGrenades = 0;
    i32 flashbangs = 0;
    i32 smokeGrenades = 0;
    
    // Equipment
    bool hasDefuser = false;
    bool hasNightVision = false;
};

/**
 * @brief Active weapon state
 */
struct WeaponStateComponent {
    u8 weaponId = 0;
    
    f32 nextPrimaryAttack = 0.0f;   // Time until next primary attack
    f32 nextSecondaryAttack = 0.0f; // Time until next secondary attack
    f32 reloadTime = 0.0f;          // Time until reload complete
    
    bool isReloading = false;
    bool isBurstMode = false;       // For Glock/Famas burst mode
    bool isScoped = false;          // For AWP/Scout/etc.
    
    i32 shotsFired = 0;             // For recoil pattern
    f32 accuracy = 1.0f;            // Current accuracy (affected by movement)
    
    Vec2 recoilOffset{0.0f};        // Current recoil offset
};

/**
 * @brief Spectator state
 */
struct SpectatorComponent {
    entt::entity target = entt::null;  // Entity being spectated
    
    enum class Mode : u8 {
        Free,       // Free camera
        FirstPerson,
        ThirdPerson,
        Chase
    };
    Mode mode = Mode::Free;
};

/**
 * @brief Round-specific player state
 */
struct RoundStateComponent {
    Vec3 spawnPosition{0.0f};
    bool hasPlantedBomb = false;
    bool hasDefused = false;
    i32 hostagesRescued = 0;
    f32 roundDamageDealt = 0.0f;
};

} // namespace cscpp::ecs

