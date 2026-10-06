#pragma once
#include <array>
#include <fstream>
#include <string>
#include <vector>

namespace mapview {
struct Army { int color = -1; int count = 0; };
class Troops {
public:
    static constexpr int MaxCount = 9999;
    using Board = std::array<Army, 43>;
    Board board{};
    std::vector<Board> history;
    int color = 0, batch = 1;
    std::string message = "Choose a color. Click a territory.";
    bool change(int id, int operation) {
        if (id < 1 || id > 42) return false;
        Army next = board[id];
        if (operation == 1) {
            if (next.count && next.color != color) {
                message = "Other army: use SET COLOR first."; return false;
            }
            if (next.count > MaxCount - batch) {
                message = "Maximum is 9999 troops."; return false;
            }
            next.color = color; next.count += batch;
        } else if (operation == -1) {
            next.count = next.count > batch ? next.count - batch : 0;
            if (!next.count) next.color = -1;
        } else if (operation == 2 && next.count) next.color = color;
        else if (operation == 0) next = Army{};
        if (next.count == board[id].count && next.color == board[id].color) return false;
        if (history.size() == 100) history.erase(history.begin());
        history.push_back(board); board[id] = next;
        message = "Army updated. Z to undo."; return true;
    }
    void undo() {
        if (history.empty()) { message = "Nothing to undo."; return; }
        board = history.back(); history.pop_back(); message = "Last change undone.";
    }
    bool save(const std::string& path) const {
        std::ofstream out(path);
        out << "RISK_TROOPS_V1\n";
        for (int id=1; id<=42; ++id) out << id << ' ' << board[id].color << ' ' << board[id].count << '\n';
        out.flush(); return bool(out);
    }
    bool load(const std::string& path) {
        std::ifstream in(path); std::string magic; Board candidate{};
        if (!(in >> magic) || magic != "RISK_TROOPS_V1") return false;
        for (int id=1; id<=42; ++id) {
            int found; auto& a=candidate[id];
            if (!(in >> found >> a.color >> a.count) || found != id || a.count < 0 || a.count > MaxCount ||
                (a.count == 0 ? a.color != -1 : a.color < 0 || a.color >= 6)) return false;
        }
        std::string extra; if (in >> extra) return false;
        board=candidate; history.clear(); message="Saved armies restored."; return true;
    }
};
}
