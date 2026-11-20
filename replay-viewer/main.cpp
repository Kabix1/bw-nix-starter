#include "ui/ui.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <thread>

namespace bwgame { namespace ui {
void log_str(a_string s) { std::fwrite(s.data(), 1, s.size(), stdout); }
void fatal_error_str(a_string s) { std::fprintf(stderr, "%s\n", s.c_str()); std::terminate(); }
}}

// A small replay-only overlay. It reads the simulated state at the displayed frame.
struct ReplayUI : bwgame::ui_functions {
    using bwgame::ui_functions::ui_functions;
    bwgame::string_table_data labels;
    bwgame::a_vector<uint8_t> resource_icons;
    std::array<std::string, 12> bot_names;

    std::string label(size_t index, const std::string& fallback) const {
        if (labels.data.size() < 2 || !index) return fallback;
        size_t count = labels.data[0] | (size_t(labels.data[1]) << 8);
        if (index > count || 2 + index * 2 > labels.data.size()) return fallback;
        size_t offset = labels.data[2 + (index - 1) * 2] | (size_t(labels.data[3 + (index - 1) * 2]) << 8);
        std::string result;
        while (offset < labels.data.size() && labels.data[offset]) {
            unsigned char c = labels.data[offset++];
            if (c >= 32 && c < 127) result += char(c);
        }
        return result.empty() ? fallback : result;
    }

    void draw_callback(uint8_t* data, size_t pitch) override {
        if (!draw_ui_elements) return;
        // Five-by-seven bitmap glyphs, scaled for legibility.
        static const char* glyphs[] = {
            "01110100011001110101110011000101110", "00100011000010000100001000010001110",
            "01110100010000100010001000100011111", "11110000010000101110000010000111110",
            "00010001100101010010111110001000010", "11111100001000011110000010000111110",
            "01110100001000011110100011000101110", "11111000010001000100010000100001000",
            "01110100011000101110100011000101110", "01110100011000101111000010000101110",
            "01110100011000111111100011000110001", "11110100011000111110100011000111110",
            "01111100001000010000100001000001111", "11110100011000110001100011000111110",
            "11111100001000011110100001000011111", "11111100001000011110100001000010000",
            "01111100001000010111100011000101111", "10001100011000111111100011000110001",
            "01110001000010000100001000010001110", "00111000100001000010100101001001100",
            "10001100101010011000101001001010001", "10000100001000010000100001000011111",
            "10001110111010110101100011000110001", "10001110011010110011100011000110001",
            "01110100011000110001100011000101110", "11110100011000111110100001000010000",
            "01110100011000110001101011001001101", "11110100011000111110101001001010001",
            "01111100001000001110000010000111110", "11111001000010000100001000010000100",
            "10001100011000110001100011000101110", "10001100011000110001100010101000100",
            "10001100011000110101101011101110001", "10001100010101000100010101000110001",
            "10001100010101000100001000010000100", "11111000010001000100010001000011111"
        };
        auto box = [&](int x, int y, int w, int h, uint8_t color) {
            for (int yy = std::max(0, y); yy < std::min<int>(screen_height, y + h); ++yy)
                for (int xx = std::max(0, x); xx < std::min<int>(screen_width, x + w); ++xx)
                    data[yy * pitch + xx] = color;
        };
        auto text = [&](int x, int y, const std::string& str, uint8_t color) {
            for (unsigned char raw : str) {
                char c = char(std::toupper(raw));
                const char* g = c >= '0' && c <= '9' ? glyphs[c - '0'] :
                                c >= 'A' && c <= 'Z' ? glyphs[10 + c - 'A'] : nullptr;
                if (g) for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
                    if (g[row * 5 + col] == '1') box(x + col * 2, y + row * 2, 2, 2, color);
                if (c == ':' || c == '.') { box(x + 4, y + 10, 2, 2, color); if (c == ':') box(x + 4, y + 4, 2, 2, color); }
                if (c == '-') box(x + 2, y + 6, 6, 2, color);
                if (c == '/') for (int i = 0; i < 5; ++i) box(x + 8 - i * 2, y + 2 + i * 2, 2, 2, color);
                if (c == '%') { box(x, y + 2, 2, 2, color); box(x + 8, y + 10, 2, 2, color); for (int i = 0; i < 5; ++i) box(x + 8 - i * 2, y + 2 + i * 2, 2, 2, color); }
                x += 12;
                if (x + 10 >= int(screen_width)) break;
            }
        };
        // game/icons.grp stores raw indexed pixels, unlike regular compressed GRPs.
        auto icon = [&](int frame, int x, int y) {
            const auto& d = resource_icons;
            auto u16 = [&](size_t p) { return unsigned(d[p]) | (unsigned(d[p + 1]) << 8); };
            if (d.size() < 6 || frame >= int(u16(0))) return;
            size_t h = 6 + size_t(frame) * 8;
            if (h + 8 > d.size()) return;
            unsigned w = d[h + 2], height = d[h + 3];
            size_t offset = u16(h + 4) | (size_t(u16(h + 6)) << 16);
            if (offset > d.size() || size_t(w) * height > d.size() - offset) return;
            for (unsigned yy = 0; yy < height; ++yy)
                for (unsigned xx = 0; xx < w; ++xx)
                    box(x + int(xx), y + int(yy), 1, 1, d[offset + yy * w + xx]);
        };
        int rows = 0;
        for (int i = 0; i < 12; ++i) if (!replay_st.player_name[i].empty()) ++rows;
        box(8, 8, 760, 8 + rows * 19, 1);
        int y = 12;
        for (int i = 0; i < 12; ++i) if (!replay_st.player_name[i].empty()) {
            std::string name(replay_st.player_name[i].begin(), replay_st.player_name[i].end());
            if (!bot_names[i].empty() && (name == "test" || name == "Test" || name == "opponent" || name == "Opponent")) name = bot_names[i];
            if (name.size() > 18) name.resize(18);
            int race = int(st.players[i].race);
            if (race < 0 || race > 2) race = 0;
            text(14, y, name, 255);
            icon(0, 240, y); text(258, y, std::to_string(st.current_minerals[i]), 255);
            icon(1 + race, 350, y); text(368, y, std::to_string(st.current_gas[i]), 255);
            icon(4 + race, 460, y);
            auto supply = [](bwgame::fp1 value) {
                return std::to_string(value.integer_part()) + ((value.raw_value & 1) ? ".5" : "");
            };
            text(478, y, supply(st.supply_used[i][race]) + "/" + supply(std::min(st.supply_available[i][race], bwgame::fp1::integer(200))), 255);
            y += 19;
        }
        if (current_selection.size() != 1) return;
        auto* u = get_unit(current_selection.front());
        if (!u || !ut_building(u->unit_type)) return;
        int panelY = std::max(80, int(screen_height) - 126);
        int panelX = std::max(8, get_minimap_area().to.x + 8);
        box(panelX, panelY, 540, 112, 1);
        auto unit_name = [&](const bwgame::unit_type_t* type) {
            return label(size_t(type->id) + 1, "UNIT " + std::to_string(int(type->id)));
        };
        text(panelX + 6, panelY + 6, unit_name(u->unit_type), 255);
        std::string activity = "IDLE";
        int remaining = 0, total = 0;
        if (!u_completed(u)) {
            activity = "CONSTRUCTING";
            remaining = u->remaining_build_time;
            total = u->unit_type->build_time;
        } else if (u->building.researching_type) {
            auto* tech = u->building.researching_type;
            activity = "RESEARCH: " + label(tech->label, "TECH " + std::to_string(int(tech->id)));
            remaining = u->building.upgrade_research_time;
            total = tech->research_time;
        } else if (u->building.upgrading_type) {
            auto* upgrade = u->building.upgrading_type;
            activity = "UPGRADE: " + label(upgrade->label, "UPGRADE " + std::to_string(int(upgrade->id)));
            remaining = u->building.upgrade_research_time;
            total = upgrade->time_cost_base + upgrade->time_cost_factor * (u->building.upgrading_level - 1);
        } else if (!u->build_queue.empty()) {
            activity = "TRAINING: " + unit_name(u->build_queue.front());
            total = u->build_queue.front()->build_time;
            remaining = u->current_build_unit ? u->current_build_unit->remaining_build_time : total;
            if (!u->current_build_unit) activity = "WAITING: " + unit_name(u->build_queue.front());
        }
        text(panelX + 6, panelY + 27, activity, 255);
        if (total > 0) {
            int percent = std::max(0, std::min(100, 100 - int(100LL * remaining / total)));
            box(panelX + 6, panelY + 49, 300, 12, 12);
            box(panelX + 8, panelY + 51, 296 * percent / 100, 8, 50);
            text(panelX + 318, panelY + 48, std::to_string(percent) + "%", 255);
        }
        if (u->build_queue.size() > 1) {
            std::string queue = "QUEUE:";
            for (size_t i = 1; i < u->build_queue.size(); ++i) queue += " " + unit_name(u->build_queue[i]);
            text(panelX + 6, panelY + 72, queue, 255);
        }
    }
};

struct Snapshot {
    bwgame::state state;
    bwgame::action_state actions;
    std::array<bwgame::apm_t, 12> apm;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: replay-viewer REPLAY DATA_DIRECTORY [--frames N,N --center X Y --output DIR --size W H --select-type ID]\n");
        return 1;
    }
    using namespace bwgame;
    try {
        std::vector<int> frames;
        std::filesystem::path output;
        int width = 960, height = 640, selectType = -1;
        xy center{-1, -1};
        auto argument = [&](int& i) -> const char* {
            if (++i >= argc) throw std::runtime_error("missing export option argument");
            return argv[i];
        };
        for (int i = 3; i < argc; ++i) {
            std::string option = argv[i];
            if (option == "--frames") {
                std::stringstream list(argument(i));
                std::string value;
                while (std::getline(list, value, ',')) frames.push_back(std::stoi(value));
            } else if (option == "--center") {
                center.x = std::stoi(argument(i)); center.y = std::stoi(argument(i));
            } else if (option == "--output") output = argument(i);
            else if (option == "--size") {
                width = std::stoi(argument(i)); height = std::stoi(argument(i));
            } else if (option == "--select-type") selectType = std::stoi(argument(i));
            else throw std::runtime_error("unknown option: " + option);
        }
        const bool exporting = argc > 3;
        if (exporting && (frames.empty() || output.empty() || width < 64 || height < 64 || width > 4096 || height > 4096))
            throw std::runtime_error("export requires frames, an output directory, and a size between 64 and 4096");
        auto load = data_loading::data_files_directory(argv[2]);
        game_player player(load);
        ReplayUI ui(std::move(player));
        ui.load_data_file = [&](a_vector<uint8_t>& data, a_string filename) { load(data, std::move(filename)); };
        if (exporting) {
            ui.create_window = false;
            ui.draw_ui_elements = false;
            ui.global_volume = 0;
        }
        ui.init();
        if (!exporting) {
            load(ui.labels.data, "rez/stat_txt.tbl");
            load(ui.resource_icons, "game/icons.grp");
        }
        ui.load_replay_file(argv[1]);
        for (int i = 0; i < 12; ++i) {
            std::string key = "BW_REPLAY_PLAYER_" + std::to_string(ui.action_st.player_id[i]);
            if (const char* name = std::getenv(key.c_str())) ui.bot_names[i] = name;
        }
        if (exporting) {
            std::sort(frames.begin(), frames.end());
            frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
            if (frames.front() < 0 || frames.back() > ui.replay_st.end_frame)
                throw std::runtime_error("export frame is outside the replay");
            ui.resize(width, height);
            ui.screen_pos = center.x < 0 ? xy{(int)ui.game_st.map_width / 2 - width / 2, (int)ui.game_st.map_height / 2 - height / 2}
                                          : xy{center.x - width / 2, center.y - height / 2};
            ui.set_image_data();
            std::filesystem::create_directories(output);
            for (int frame : frames) {
                while (ui.st.current_frame < frame) ui.replay_functions::next_frame();
                ui.replay_frame = frame;
                ui.current_selection_clear();
                for (int owner = 0; owner < 12; ++owner)
                    for (auto& unit : ui.st.player_units[owner])
                        if ((int)unit.unit_type->id == selectType) ui.current_selection_add(&unit);
                ui.update();
                auto path = output / ("frame-" + std::to_string(frame) + ".ppm");
                std::ofstream file(path, std::ios::binary);
                if (!file) throw std::runtime_error("cannot write " + path.string());
                file << "P6\n" << width << " " << height << "\n255\n";
                auto* pixels = static_cast<const unsigned char*>(ui.rgba_surface->lock());
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                        file.write(reinterpret_cast<const char*>(pixels + y * ui.rgba_surface->pitch + x * 4), 3);
                ui.rgba_surface->unlock();
                std::printf("Rendered frame %d, camera (%d,%d): %s\n", frame, ui.screen_pos.x, ui.screen_pos.y, path.c_str());
                for (int owner = 0; owner < 2; ++owner)
                    for (const auto& unit : ui.st.player_units[owner])
                        if (unit.position.x >= ui.screen_pos.x && unit.position.x < ui.screen_pos.x + width &&
                            unit.position.y >= ui.screen_pos.y && unit.position.y < ui.screen_pos.y + height)
                            std::printf("  unit owner=%d type=%d position=(%d,%d) id=%u hp=%d shields=%d completed=%d disabled=%d kills=%d cooldown=%d carried=%d\n",
                                owner, (int)unit.unit_type->id, unit.position.x, unit.position.y,
                                (unsigned)ui.get_unit_id(&unit).raw_value, unit.hp.integer_part(), unit.shield_points.integer_part(),
                                ui.u_completed(&unit), ui.u_disabled(&unit), unit.kill_count, unit.ground_weapon_cooldown,
                                (int)unit.unit_type->id == 64 ? unit.worker.resources_carried : 0);
            }
            return 0;
        }
        ui.resize(1280, 800);
        ui.screen_pos = {(int)ui.game_st.map_width / 2 - 640, (int)ui.game_st.map_height / 2 - 400};
        ui.set_image_data();
        std::map<int, std::unique_ptr<Snapshot>> snapshots;
        auto save = [&] {
            auto s = std::make_unique<Snapshot>();
            s->state = copy_state(ui.st);
            s->actions = copy_state(ui.action_st, ui.st, s->state);
            s->apm = ui.apm;
            snapshots.emplace(ui.st.current_frame, std::move(s));
        };
        auto advance = [&] {
            ui.replay_functions::next_frame();
            for (auto& apm : ui.apm) apm.update(ui.st.current_frame);
            if (ui.st.current_frame % 240 == 0 && !snapshots.count(ui.st.current_frame)) save();
        };
        save();
        auto last = std::chrono::steady_clock::now();
        std::printf("Replay loaded: %s (%d frames)\n", argv[1], ui.replay_st.end_frame);
        std::fflush(stdout);
        while (true) {
            auto now = std::chrono::steady_clock::now();
            int target = std::max(0, std::min(ui.replay_frame, ui.replay_st.end_frame));
            if (target != ui.st.current_frame) {
                auto it = snapshots.upper_bound(target);
                --it;
                if (ui.st.current_frame > target || it->first > ui.st.current_frame) {
                    auto& s = *it->second;
                    ui.st = copy_state(s.state);
                    ui.action_st = copy_state(s.actions, s.state, ui.st);
                    ui.apm = s.apm;
                }
                for (int i = 0; i < 128 && ui.st.current_frame < target; ++i) advance();
                last = now;
            } else if (!ui.is_paused && !ui.is_done()) {
                auto delay = std::chrono::milliseconds(std::max<std::chrono::milliseconds::rep>(1, (fp8::integer(42) / ui.game_speed).integer_part()));
                int frames = std::min(16, (int)((now - last) / delay));
                for (int i = 0; i < frames && !ui.is_done(); ++i) advance();
                if (frames) { ui.replay_frame = ui.st.current_frame; last = now; }
            } else last = now;
            ui.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "replay-viewer: %s\n", e.what());
        return 1;
    }
}
