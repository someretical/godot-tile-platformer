#include "player.h"
#include "level.h"
#include "tile.h"
#include "brush.h"
#include "tile_data.h"
#include "mapdata.h"
#include "console.h"
#include "marker.h"

#include <cmath>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/input_event_action.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

using namespace godot;

void Level::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_camera_pos"), &Level::get_camera_pos);
	ClassDB::bind_method(D_METHOD("set_camera_pos", "pos"), &Level::set_camera_pos);
    ClassDB::add_property("Level", PropertyInfo(Variant::VECTOR2, "m_camera_true_pos"), "set_camera_pos", "get_camera_pos");
}

Level::Level() {
    set_process_priority(static_cast<int>(ProcessingPriority::Level));

    /* prevent process from running in background editor */
    if (Engine::get_singleton()->is_editor_hint()) {
        set_process_mode(ProcessMode::PROCESS_MODE_DISABLED);
    }

    m_rng = memnew(RandomNumberGenerator);
    if (!m_rng) {
        std::exit(1);
    }
    m_rng->randomize();

    const auto loader = ResourceLoader::get_singleton();

    m_tile_preloader = memnew(ResourcePreloader);
    if (!m_tile_preloader) {
        std::exit(1);
    }
    m_tile_preloader->set_name("Tile Preloader");
    m_tile_preloader->add_resource("Blue-1", loader->load("src/assets/tiles/blue/Blue-1.png"));
    m_tile_preloader->add_resource("Blue-2", loader->load("src/assets/tiles/blue/Blue-2.png"));
    m_tile_preloader->add_resource("Blue-3", loader->load("src/assets/tiles/blue/Blue-3.png"));
    m_tile_preloader->add_resource("Blue-4", loader->load("src/assets/tiles/blue/Blue-4.png"));
    m_tile_preloader->add_resource("Blue-5", loader->load("src/assets/tiles/blue/Blue-5.png"));
    m_tile_preloader->add_resource("Blue-6", loader->load("src/assets/tiles/blue/Blue-6.png"));
    m_tile_preloader->add_resource("Blue-7", loader->load("src/assets/tiles/blue/Blue-7.png"));
    m_tile_preloader->add_resource("Blue-8", loader->load("src/assets/tiles/blue/Blue-8.png"));
    m_tile_preloader->add_resource("Blue-9", loader->load("src/assets/tiles/blue/Blue-9.png"));
    m_tile_preloader->add_resource("Block-Gold", loader->load("src/assets/tiles/Block-Gold.png"));
    m_tile_preloader->add_resource("Block-Wood", loader->load("src/assets/tiles/wood/Block-Wood.png"));
    m_tile_preloader->add_resource("Wood-0", loader->load("src/assets/tiles/wood/Wood-0.png"));
    m_tile_preloader->add_resource("Wood-1", loader->load("src/assets/tiles/wood/Wood-1.png"));
    m_tile_preloader->add_resource("Wood-2", loader->load("src/assets/tiles/wood/Wood-2.png"));
    m_tile_preloader->add_resource("Wood-3", loader->load("src/assets/tiles/wood/Wood-3.png"));
    m_tile_preloader->add_resource("Wood-4", loader->load("src/assets/tiles/wood/Wood-4.png"));
    m_tile_preloader->add_resource("Wood-5", loader->load("src/assets/tiles/wood/Wood-5.png"));
    m_tile_preloader->add_resource("Coin1", loader->load("src/assets/entities/coin/coin1.png"));
    m_tile_preloader->add_resource("Coin2", loader->load("src/assets/entities/coin/coin2.png"));
    m_tile_preloader->add_resource("Coin3", loader->load("src/assets/entities/coin/coin3.png"));
    m_tile_preloader->add_resource("Coin4", loader->load("src/assets/entities/coin/coin4.png"));
    m_tile_preloader->add_resource("Question1", loader->load("src/assets/tiles/question/Question1.png"));
    m_tile_preloader->add_resource("Question2", loader->load("src/assets/tiles/question/Question2.png"));
    m_tile_preloader->add_resource("Question3", loader->load("src/assets/tiles/question/Question3.png"));
    m_tile_preloader->add_resource("Question4", loader->load("src/assets/tiles/question/Question4.png"));
    m_tile_preloader->add_resource("StartPos", loader->load("src/assets/tiles/special/start_pos.png"));
    add_child(m_tile_preloader);

    m_hud_layer = memnew(CanvasLayer);
    if (!m_hud_layer) {
        std::exit(1);
    }
    m_hud_layer->set_name("HUD layer");
    m_hud_layer->set_layer(HUD_LAYER);
    m_hud_layer->set_offset(Vector2{CAMERA_WIDTH / 2, CAMERA_HEIGHT / 2});
    add_child(m_hud_layer);

    m_console = memnew(Console(this));
    if (!m_console) {
        std::exit(1);
    }
    m_console->set_name("Console");
    m_hud_layer->add_child(m_console);

    m_game_layer = memnew(CanvasLayer);
    if (!m_game_layer) {
        std::exit(1);
    }
    m_game_layer->set_name("Game layer");
    m_game_layer->set_layer(GAME_LAYER);
    m_game_layer->set_offset(Vector2{CAMERA_WIDTH / 2, CAMERA_HEIGHT / 2});
    add_child(m_game_layer);

    m_editor.m_brush = memnew(Brush(this));
    if (!m_editor.m_brush) {
        std::exit(1);
    }
    m_editor.m_brush->set_visible(false);
    m_game_layer->add_child(m_editor.m_brush);

    m_editor.m_start_pos = memnew(Marker(this, Vector2{0, 0}, m_tile_preloader->get_resource("StartPos")));
    if (!m_editor.m_start_pos) {
        std::exit(1);
    }
    m_editor.m_start_pos->set_visible(false);
    m_editor.m_enabled = false;
    m_game_layer->add_child(m_editor.m_start_pos);

    /* Setup camera */
    m_camera = memnew(Camera2D);
    if (!m_camera) {
        std::exit(1);
    }
    m_camera->set_name("Camera");
    m_camera->set_zoom(Vector2(SCREEN_ZOOM, SCREEN_ZOOM));
    add_child(m_camera);

    m_tiles_node = NULL;
    m_collectables_node = NULL;
    m_mobs_node = NULL;
    m_particles_node = NULL;
    m_player = NULL;

    load_level("res://src/assets/levels/1.json");
}

Level::~Level() {
    memdelete(m_rng);
    queue_free();
}

void Level::set_camera_pos(const Vector2 pos) {
    m_camera_true_pos = pos;
}

Vector2 Level::get_camera_pos() const {
    return m_camera_true_pos;
}

void Level::update_camera() {
    /* update camera location in level */
    m_camera_true_pos.x = m_player->m_true_pos.x;
    /* vertical movement of camera lags behind player position to prevent jerkiness */
    m_camera_true_pos.y += (m_player->m_true_pos.y - m_camera_true_pos.y) / 4;

    if (m_camera_true_pos.x < CAMERA_WIDTH / 2) {
        m_camera_true_pos.x = CAMERA_WIDTH / 2;
    } else if (m_camera_true_pos.x > m_curmap->m_dimensions.x * TILE_SIZE - CAMERA_WIDTH / 2) {
        m_camera_true_pos.x = m_curmap->m_dimensions.x * TILE_SIZE - CAMERA_WIDTH / 2;
    }

    if (m_camera_true_pos.y < CAMERA_HEIGHT / 2) {
        m_camera_true_pos.y = CAMERA_HEIGHT / 2;
    } else if (m_camera_true_pos.y > m_curmap->m_dimensions.y * TILE_SIZE - CAMERA_HEIGHT / 2) {
        m_camera_true_pos.y = m_curmap->m_dimensions.y * TILE_SIZE - CAMERA_HEIGHT / 2;
    }
}

Error Level::load_level(const String &path) {
    if (m_tiles_node) m_tiles_node->queue_free();
    m_tiles_node = memnew(Node);
    if (!m_tiles_node) {
        std::exit(1);
    }
    m_tiles_node->set_name("Tiles");
    m_game_layer->add_child(m_tiles_node);

    if (m_collectables_node) m_collectables_node->queue_free();
    m_collectables_node = memnew(Node);
    if (!m_collectables_node) {
        std::exit(1);
    }
    m_collectables_node->set_name("Collectables");
    m_game_layer->add_child(m_collectables_node);

    if (m_mobs_node) m_mobs_node->queue_free();
    m_mobs_node = memnew(Node);
    if (!m_mobs_node) {
        std::exit(1);
    }
    m_mobs_node->set_name("Mobs");
    m_game_layer->add_child(m_mobs_node);

    if (m_particles_node) m_particles_node->queue_free();
    m_particles_node = memnew(Node);
    if (!m_particles_node) {
        std::exit(1);
    }
    m_particles_node->set_name("Particles");
    m_game_layer->add_child(m_particles_node);

    if (m_player) m_player->queue_free();
    m_player = memnew(Player(this, Vector2{CAMERA_WIDTH / 2, CAMERA_HEIGHT / 2}));
    if (!m_player) {
        std::exit(1);
    }
    m_mobs_node->add_child(m_player);

    if (auto map = MapData::load_map(path); map.has_value()) {
        m_curmap = map.value();
        m_bounds = Rect2{
            0 + TINY, 
            0 + TINY, 
            static_cast<float>(m_curmap->m_dimensions.x * TILE_SIZE) - (2 * TINY), 
            static_cast<float>(m_curmap->m_dimensions.y * TILE_SIZE) - (2 * TINY)
        };

        /* the true position is the position of the player, tiles, camera within the level itself, not the window */
        m_editor.m_start_pos->m_true_pos = m_curmap->m_start_pos;
        m_player->m_true_pos = m_curmap->m_start_pos * TILE_SIZE + Vector2{HALF_TILE, HITBOX_BOTTOM_GAP};
        m_camera_true_pos.x = UtilityFunctions::clampf(m_player->m_true_pos.x, m_bounds.get_position().x + CAMERA_WIDTH / 2, m_bounds.get_end().x - CAMERA_WIDTH / 2);
        m_camera_true_pos.y = UtilityFunctions::clampf(m_player->m_true_pos.y, m_bounds.get_position().y + CAMERA_HEIGHT / 2, m_bounds.get_end().y - CAMERA_HEIGHT / 2);

        /* find the offset of the camera's true position in the tile grid as the camera can be between grid lines */
        const Vector2 camera_offset{fmodf(m_camera_true_pos.x, TILE_SIZE), fmodf(m_camera_true_pos.y, TILE_SIZE)};
        
        /* find the grid tile the camera's true position lies in */
        /* the i suffix indicates that these are indices in the tile grid */
        const Vector2i centre_pos_i{
            static_cast<int>(m_camera_true_pos.x / TILE_SIZE),
            static_cast<int>(m_camera_true_pos.y / TILE_SIZE)
        };

        /* go from centre to top left */
        const Vector2i top_left_pos_i{
            static_cast<int>((m_camera_true_pos.x - (CAMERA_WIDTH / 2)) / TILE_SIZE),
            static_cast<int>((m_camera_true_pos.y - (CAMERA_HEIGHT / 2)) / TILE_SIZE)
        };

        /* i and j correspond to the RELATIVE tile grid indices from the top left of the window */
        for (int j = 0; j < TILE_COUNT_Y; j++) {
            for (int i = 0; i < TILE_COUNT_X; i++) {
                auto tile = memnew(Tile(this, Vector2i(i, j) + top_left_pos_i));
                if (!tile) {
                    std::exit(1);
                }
                m_tiles_node->add_child(tile);
            }
        }

        return OK;
    } else {
        return map.error();
    }
}

Error Level::import_mapdata_inplace(const String &path) {
    if (auto map = MapData::load_map(path); map.has_value()) {
        m_curmap = map.value();
        m_bounds = Rect2{
            0 + TINY, 
            0 + TINY, 
            static_cast<float>(m_curmap->m_dimensions.x * TILE_SIZE) - (2 * TINY), 
            static_cast<float>(m_curmap->m_dimensions.y * TILE_SIZE) - (2 * TINY)
        };
        return OK;
    } else {
        return map.error();
    }
}

Error Level::export_current_map(const String &path) {
    return m_curmap->save_map(path);
}

void Level::_unhandled_input(const Ref<InputEvent> &event) {
    if (handle_console_open(event)) return;
    if (handle_editor_toggle(event)) return;
}

bool Level::handle_console_open(const Ref<InputEvent> &event) {
    if (event->is_action_pressed("ui_open_console")) {
        m_console->set_visible(true);
        m_console->m_line_edit->grab_focus();
        get_viewport()->set_input_as_handled();
        get_tree()->set_pause(true);
        return true;
    }

    return false;
}

bool Level::handle_editor_toggle(const Ref<InputEvent> &event) {
    if (event->is_action_pressed("ui_toggle_level_editor")) {
        m_editor.m_enabled = !m_editor.m_enabled;
        m_editor.m_brush->set_visible(m_editor.m_enabled);
        m_editor.m_brush->set_process_mode(m_editor.m_enabled ? ProcessMode::PROCESS_MODE_INHERIT : ProcessMode::PROCESS_MODE_DISABLED);
        m_editor.m_start_pos->set_visible(m_editor.m_enabled);
        m_editor.m_start_pos->set_process_mode(m_editor.m_enabled ? ProcessMode::PROCESS_MODE_INHERIT : ProcessMode::PROCESS_MODE_DISABLED);
        get_viewport()->set_input_as_handled();
        return true;
    }

    return false;
}

void Level::_ready() {
    RenderingServer::get_singleton()->set_default_clear_color(Color::hex(0xA0D0F8FF));
}

void Level::_process(double delta) {
    m_time += delta;
}

int Level::get_tile_frame_mod4() const {
    return static_cast<long>(m_time * 8) % 4;
}