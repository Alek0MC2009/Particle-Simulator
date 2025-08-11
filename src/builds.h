#ifndef BUILDS_H
#define BUILDS_H

#include <string>
#include <vector>
#include <fstream>

struct Build {
    std::string name;
    int cost;
    int count;
    int unlockScore; // Puntaje para desbloquear
    int cps;         // clicks per second que aporta
};

inline void SaveGame(int score, const std::vector<Build>& builds, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    file << score << "\n";
    for (const auto& b : builds) {
        file << b.count << "\n";
    }
    file.close();
}

inline void LoadGame(int& score, std::vector<Build>& builds, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    file >> score;
    for (auto& b : builds) {
        file >> b.count;
    }
    file.close();
}

#endif // BUILDS_H
