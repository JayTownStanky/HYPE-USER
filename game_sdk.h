#ifndef GAME_SDK_H
#define GAME_SDK_H

#include "mem.h"
#include "config.h"
#include <cstdint>
#include <vector>
#include <cmath>

// 3D Vector
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    float length_2d() const {
        return std::sqrt(x * x + y * y);
    }

    Vec3 normalize() const {
        float len = length();
        if (len > 0.0001f) {
            return Vec3(x / len, y / len, z / len);
        }
        return Vec3();
    }

    float dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    bool is_zero() const {
        return x == 0.0f && y == 0.0f && z == 0.0f;
    }
};

// 2D Vector (for screen positions)
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    float length() const {
        return std::sqrt(x * x + y * y);
    }
};

// View matrix (4x4)
struct ViewMatrix {
    float m[4][4] = {};

    bool world_to_screen(const Vec3& world, Vec2& screen, int width, int height) const {
        float w = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];
        if (w < 0.001f) return false;

        float inv_w = 1.0f / w;
        float sx = m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3];
        float sy = m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3];

        screen.x = (width / 2.0f) + (sx * inv_w) * (width / 2.0f);
        screen.y = (height / 2.0f) - (sy * inv_w) * (height / 2.0f);

        return true;
    }
};

// Entity data
struct Entity {
    uint64_t address = 0;
    Vec3 position;
    Vec3 head_position;
    int32_t health = 0;
    int32_t max_health = 100;
    int32_t team = 0;
    bool dormant = true;

    bool alive() const {
        return health > 0 && !dormant;
    }

    bool valid() const {
        return address != 0 && !position.is_zero();
    }

    float distance_to(const Vec3& other) const {
        return (position - other).length();
    }
};

// Game state manager
class GameState {
public:
    GameState() = default;
    ~GameState() = default;

    // Initialize - attach to game process
    bool init(HvMemory& mem, const Config& cfg);

    // Update game state (call each frame)
    bool update();

    // Get local player
    const Entity& local_player() const { return m_local; }

    // Get all entities
    const std::vector<Entity>& entities() const { return m_entities; }

    // Get current view matrix
    const ViewMatrix& view_matrix() const { return m_view_matrix; }

    // Check if initialized
    bool ready() const { return m_ready; }

    // Get module base
    uint64_t client_base() const { return m_client_base; }
    uint64_t engine_base() const { return m_engine_base; }

private:
    bool read_local_player();
    bool read_entities();
    bool read_view_matrix();
    Entity read_entity(uint64_t address);

    HvMemory* m_mem = nullptr;
    const Config* m_cfg = nullptr;

    bool m_ready = false;
    uint64_t m_client_base = 0;
    uint64_t m_engine_base = 0;

    Entity m_local;
    std::vector<Entity> m_entities;
    ViewMatrix m_view_matrix;

    // Cached global offsets (from config.json)
    uint64_t m_off_entity_list = 0;
    uint64_t m_off_local_player = 0;
    uint64_t m_off_view_matrix = 0;
    uint64_t m_off_name_list = 0;
    uint64_t m_off_level = 0;
    uint64_t m_off_glow_highlights = 0;
};

#endif // GAME_SDK_H
