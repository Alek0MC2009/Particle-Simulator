#include "raylib.h"
#include <vector>
#include <cstdlib>
#include <string>
#include <fstream>
#include <cmath>
#include <algorithm>

const int GRID_WIDTH = 120;
const int GRID_HEIGHT = 60;
const int CELL_SIZE = 8;
const int UI_HEIGHT = 150;
const int WINDOW_WIDTH = GRID_WIDTH * CELL_SIZE;
const int WINDOW_HEIGHT = GRID_HEIGHT * CELL_SIZE + UI_HEIGHT;

enum class Particle {
    Empty,
    Sand,
    Water,
    Lava,
    Stone,
    Steam,
    Ice,
    Acid,
    Oil,
    Fire,
    Smoke,
    Obsidian
};

struct ColorMap {
    Particle type;
    Color color;
    char symbol;
    std::string name;
};

// Mapeo de colores y símbolos para partículas
const ColorMap colors[] = {
    { Particle::Empty,    BLACK,                      ' ', "Empty" },
    { Particle::Sand,     { 194, 178, 128, 255 },    'S', "Sand" },
    { Particle::Water,    { 64, 164, 223, 255 },     'W', "Water" },
    { Particle::Lava,     { 255, 100, 0, 255 },      'L', "Lava" },
    { Particle::Stone,    { 128, 128, 128, 255 },    '#', "Stone" },
    { Particle::Steam,    { 220, 220, 220, 180 },    'T', "Steam" },
    { Particle::Ice,      { 173, 216, 230, 255 },    'I', "Ice" },
    { Particle::Acid,     { 0, 255, 0, 255 },        'A', "Acid" },
    { Particle::Oil,      { 139, 69, 19, 255 },      'O', "Oil" },
    { Particle::Fire,     { 255, 140, 0, 255 },      'F', "Fire" },
    { Particle::Smoke,    { 105, 105, 105, 200 },    'M', "Smoke" },
    { Particle::Obsidian, { 50, 50, 50, 255 },       'B', "Obsidian" }
};

enum class BrushSize {
    Small = 1,
    Medium = 3,
    Large = 5,
    XLarge = 7
};

struct GameState {
    bool showMenu = false;
    bool showHelp = false;
    bool isPaused = false;
    float timeSpeed = 1.0f;
    BrushSize brushSize = BrushSize::Small;
    int frameCounter = 0;
    float menuTransition = 0.0f;
    float helpTransition = 0.0f;
};

class ParticleGrid {
    Particle* grid;
    Particle* nextGrid;
    int* temperature; // Para reacciones térmicas
    int width, height;
    GameState* gameState;

public:
    ParticleGrid(int w, int h, GameState* state) : width(w), height(h), gameState(state) {
        grid = new Particle[width * height]();
        nextGrid = new Particle[width * height]();
        temperature = new int[width * height]();
        Clear();
        CreateGround();
    }

    ~ParticleGrid() {
        delete[] grid;
        delete[] nextGrid;
        delete[] temperature;
    }

    void Clear() {
        for (int i = 0; i < width * height; i++) {
            grid[i] = Particle::Empty;
            nextGrid[i] = Particle::Empty;
            temperature[i] = 20; // Temperatura ambiente
        }
        CreateGround();
    }

    void CreateGround() {
        // Crear suelo rígido en las 2 filas inferiores
        for (int x = 0; x < width; x++) {
            grid[(height - 1) * width + x] = Particle::Stone;
            grid[(height - 2) * width + x] = Particle::Stone;
        }
        // Paredes laterales
        for (int y = 0; y < height - 1; y++) {
            grid[y * width + 0] = Particle::Stone;
            grid[y * width + (width - 1)] = Particle::Stone;
        }
    }

    Particle Get(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return Particle::Stone;
        return grid[y * width + x];
    }

    void Set(int x, int y, Particle p) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        
        // No permitir modificar bordes
        if (y >= height - 2 || x == 0 || x == width - 1) return;

        grid[y * width + x] = p;
    }

    int GetTemperature(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 20;
        return temperature[y * width + x];
    }

    void SetTemperature(int x, int y, int temp) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        temperature[y * width + x] = temp;
    }

    void Update() {
        if (gameState->isPaused) return;

        // Solo actualizar según la velocidad del tiempo
        gameState->frameCounter++;
        int skipFrames = std::max(1, (int)(1.0f / gameState->timeSpeed));
        if (gameState->frameCounter % skipFrames != 0) return;

        // Limpiar next grid
        for (int i = 0; i < width * height; i++) {
            nextGrid[i] = grid[i]; // Copiar estado actual primero
        }

        // Mantener bordes
        CreateGroundNext();

        // Actualizar partículas desde abajo hacia arriba
        for (int y = height - 3; y >= 0; y--) {
            for (int x = 1; x < width - 1; x++) {
                Particle p = Get(x, y);
                if (p == Particle::Empty) continue;

                // Verificar reacciones químicas primero
                if (ProcessReactions(x, y)) continue;

                // Procesar movimiento de partículas
                switch (p) {
                    case Particle::Sand:
                        UpdateSand(x, y);
                        break;
                    case Particle::Water:
                        UpdateWater(x, y);
                        break;
                    case Particle::Lava:
                        UpdateLava(x, y);
                        break;
                    case Particle::Steam:
                        UpdateSteam(x, y);
                        break;
                    case Particle::Ice:
                        UpdateIce(x, y);
                        break;
                    case Particle::Acid:
                        UpdateAcid(x, y);
                        break;
                    case Particle::Oil:
                        UpdateOil(x, y);
                        break;
                    case Particle::Fire:
                        UpdateFire(x, y);
                        break;
                    case Particle::Smoke:
                        UpdateSmoke(x, y);
                        break;
                    case Particle::Obsidian:
                        // La obsidiana es sólida, no se mueve
                        break;
                    default:
                        // La partícula ya está copiada en nextGrid
                        break;
                }
            }
        }

        // Propagar temperatura
        UpdateTemperature();

        // Intercambiar grids
        std::swap(grid, nextGrid);
    }

    void UpdateTemperature() {
        // Propagación simple de temperatura
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                if (Get(x, y) == Particle::Empty) continue;
                
                int avgTemp = GetTemperature(x, y);
                int neighbors = 1;
                
                // Promediar con vecinos
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        if (Get(x + dx, y + dy) != Particle::Empty) {
                            avgTemp += GetTemperature(x + dx, y + dy);
                            neighbors++;
                        }
                    }
                }
                
                int newTemp = avgTemp / neighbors;
                // Tender hacia temperatura ambiente
                if (newTemp > 20) newTemp = std::max(20, newTemp - 1);
                if (newTemp < 20) newTemp = std::min(20, newTemp + 1);
                
                SetTemperature(x, y, newTemp);
            }
        }
    }

    void Draw() const {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Particle p = Get(x, y);
                if (p == Particle::Empty) continue;
                
                Color c = GetColorForParticle(p);
                
                // Añadir variación de color para algunas partículas
                if (p == Particle::Fire) {
                    int variation = rand() % 50;
                    c.r = std::min(255, c.r + variation);
                    c.g = std::max(0, c.g - variation/2);
                } else if (p == Particle::Lava) {
                    int variation = rand() % 30;
                    c.r = std::max(200, c.r - variation);
                } else if (p == Particle::Steam) {
                    // Vapor semi-transparente con movimiento
                    c.a = 100 + (rand() % 80);
                } else if (p == Particle::Smoke) {
                    c.a = 150 + (rand() % 50);
                }
                
                DrawRectangle(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, c);
            }
        }
    }

    void AddParticle(int x, int y, Particle p, BrushSize brush) {
        int brushRadius = (int)brush / 2;
        
        for (int dy = -brushRadius; dy <= brushRadius; dy++) {
            for (int dx = -brushRadius; dx <= brushRadius; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                
                // Forma circular del pincel
                if (dx*dx + dy*dy <= brushRadius*brushRadius) {
                    if (nx > 0 && nx < width - 1 && ny >= 0 && ny < height - 2) {
                        if (p == Particle::Empty || Get(nx, ny) == Particle::Empty || Get(nx, ny) == Particle::Steam) {
                            Set(nx, ny, p);
                            
                            // Establecer temperatura inicial
                            switch (p) {
                                case Particle::Lava:
                                case Particle::Fire:
                                    SetTemperature(nx, ny, 1000);
                                    break;
                                case Particle::Ice:
                                    SetTemperature(nx, ny, -10);
                                    break;
                                case Particle::Steam:
                                    SetTemperature(nx, ny, 100);
                                    break;
                                default:
                                    SetTemperature(nx, ny, 20);
                                    break;
                            }
                        }
                    }
                }
            }
        }
    }

    void SaveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;
        
        // Guardar metadatos
        file << "# Particle Simulator Save File\n";
        file << "# Width: " << width << " Height: " << height << "\n";
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Particle p = Get(x, y);
                file << GetSymbolForParticle(p);
            }
            file << '\n';
        }
        file.close();
    }

    void LoadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return;

        Clear();

        std::string line;
        int y = 0;
        while (std::getline(file, line) && y < height) {
            // Saltar comentarios
            if (line.empty() || line[0] == '#') continue;
            
            for (int x = 0; x < (int)line.size() && x < width; x++) {
                Particle p = ParticleFromSymbol(line[x]);
                // No modificar bordes
                if (y >= height - 2 || x == 0 || x == width - 1) {
                    Set(x, y, Particle::Stone);
                } else {
                    Set(x, y, p);
                }
            }
            y++;
        }
        file.close();
    }

    void DrawUI(Particle currentParticle, BrushSize brushSize) const {
        int startY = GRID_HEIGHT * CELL_SIZE;
        
        // Fondo del UI con gradiente
        Color bgColor1 = { 30, 30, 35, 255 };
        Color bgColor2 = { 20, 20, 25, 255 };
        DrawRectangleGradientV(0, startY, WINDOW_WIDTH, UI_HEIGHT, bgColor1, bgColor2);
        
        // Línea decorativa superior
        DrawRectangle(0, startY, WINDOW_WIDTH, 2, { 70, 130, 200, 255 });
        
        // Título con efecto
        DrawText("PARTICLE SIMULATOR", 12, startY + 7, 20, BLACK);
        DrawText("PARTICLE SIMULATOR", 10, startY + 5, 20, { 255, 255, 255, 255 });
        
        // Panel de información actual
        int panelX = 10;
        int panelY = startY + 35;
        int panelWidth = 200;
        int panelHeight = 45;
        
        DrawRectangle(panelX, panelY, panelWidth, panelHeight, { 40, 40, 50, 200 });
        DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, { 70, 130, 200, 150 });
        
        // Muestra de la partícula actual
        Color particleColor = GetColorForParticle(currentParticle);
        DrawRectangle(panelX + 10, panelY + 8, 12, 12, particleColor);
        DrawRectangleLines(panelX + 10, panelY + 8, 12, 12, WHITE);
        
        std::string currentText = GetParticleName(currentParticle);
        DrawText(currentText.c_str(), panelX + 30, panelY + 10, 16, RAYWHITE);
        
        // Tamaño del pincel
        std::string brushText = "Brush: " + std::to_string((int)brushSize) + "px";
        DrawText(brushText.c_str(), panelX + 10, panelY + 25, 14, { 200, 200, 200, 255 });
        
        // Panel de estado del juego
        int statusX = WINDOW_WIDTH - 150;
        DrawRectangle(statusX, panelY, 140, panelHeight, { 40, 40, 50, 200 });
        DrawRectangleLines(statusX, panelY, 140, panelHeight, { 70, 130, 200, 150 });
        
        if (gameState->isPaused) {
            DrawText("PAUSED", statusX + 10, panelY + 8, 16, { 255, 100, 100, 255 });
        } else {
            DrawText("RUNNING", statusX + 10, panelY + 8, 16, { 100, 255, 100, 255 });
        }
        
        std::string speedText = "Speed: " + std::to_string(gameState->timeSpeed) + "x";
        DrawText(speedText.c_str(), statusX + 10, panelY + 25, 14, { 255, 200, 100, 255 });
        
        // Controles en la parte inferior
        int controlsY = startY + 90;
        DrawText("Controls: 1-9,0=Particles | SPACE=Pause | R=Reset | M=Menu", 10, controlsY, 12, { 180, 180, 180, 255 });
        DrawText("Mouse: L=Place R=Erase | Wheel=Brush | +/-=Speed | G=Save L=Load", 10, controlsY + 15, 12, { 160, 160, 160, 255 });
    }

    void DrawModernMenu() const {
        // Fondo semi-transparente con blur effect
        DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, { 0, 0, 0, 180 });
        
        int centerX = WINDOW_WIDTH / 2;
        int centerY = WINDOW_HEIGHT / 2;
        int menuWidth = 600;
        int menuHeight = 500;
        
        // Panel principal del menú
        int menuX = centerX - menuWidth/2;
        int menuY = centerY - menuHeight/2;
        
        DrawRectangle(menuX, menuY, menuWidth, menuHeight, { 25, 25, 35, 240 });
        DrawRectangleLines(menuX, menuY, menuWidth, menuHeight, { 70, 130, 200, 200 });
        
        // Header del menú
        DrawRectangle(menuX, menuY, menuWidth, 60, { 70, 130, 200, 100 });
        
        // Título
        DrawText("PARTICLE SIMULATOR", centerX - 140, menuY + 15, 24, WHITE);
        Color subtitleColor = { 200, 200, 200, 255 };
        DrawText("Enhanced Edition v2.0", centerX - 90, menuY + 40, 14, subtitleColor);
        
        int contentY = menuY + 80;
        int leftCol = menuX + 30;
        int rightCol = menuX + 320;
        
        // Sección de partículas
        Color particleHeaderColor = { 255, 200, 100, 255 };
        DrawText("PARTICLES", leftCol, contentY, 18, particleHeaderColor);
        
        const char* particleList[] = {
            "1: Sand - Falls and settles",
            "2: Water - Flows naturally", 
            "3: Lava - Hot liquid rock",
            "4: Stone - Solid barrier",
            "5: Steam - Rises upward",
            "6: Ice - Frozen water",
            "7: Acid - Dissolves materials",
            "8: Oil - Flammable liquid",
            "9: Fire - Burns and spreads",
            "0: Smoke - Light gas"
        };
        
        for (int i = 0; i < 10; i++) {
            Color textColor = (i < 5) ? RAYWHITE : (Color){ 220, 220, 220, 255 };
            DrawText(particleList[i], (i < 5) ? leftCol : rightCol, contentY + 25 + (i % 5) * 20, 12, textColor);
        }
        
        // Sección de controles
        int controlsY = contentY + 170;
        Color controlsHeaderColor = { 100, 255, 150, 255 };
        DrawText("CONTROLS", leftCol, controlsY, 18, controlsHeaderColor);
        
        const char* controlsList[] = {
            "Left Mouse: Place particles",
            "Right Mouse: Erase",
            "Mouse Wheel: Change brush size", 
            "SPACE: Pause/Resume simulation",
            "R: Reset simulation",
            "+/-: Adjust time speed",
            "G: Save simulation",
            "L: Load simulation"
        };
        
        for (int i = 0; i < 8; i++) {
            DrawText(controlsList[i], (i < 4) ? leftCol : rightCol, controlsY + 25 + (i % 4) * 18, 11, { 200, 200, 200, 255 });
        }
        
        // Botón de cerrar
        int closeButtonX = menuX + menuWidth - 150;
        int closeButtonY = menuY + menuHeight - 50;
        int closeButtonW = 130;
        int closeButtonH = 35;
        
        DrawRectangle(closeButtonX, closeButtonY, closeButtonW, closeButtonH, { 70, 130, 200, 200 });
        DrawRectangleLines(closeButtonX, closeButtonY, closeButtonW, closeButtonH, { 100, 160, 230, 255 });
        DrawText("Press M to Close", closeButtonX + 15, closeButtonY + 10, 14, WHITE);
        
        // Efectos decorativos
        for (int i = 0; i < 20; i++) {
            float angle = (GetTime() * 50 + i * 18) * DEG2RAD;
            int x = centerX + (int)(cos(angle) * (200 + i * 5));
            int y = centerY + (int)(sin(angle) * (150 + i * 3));
            Color dotColor = { 70, 130, 200, (unsigned char)(50 - i * 2) };
            DrawCircle(x, y, 2, dotColor);
        }
    }

private:
    void CreateGroundNext() {
        for (int x = 0; x < width; x++) {
            nextGrid[(height - 1) * width + x] = Particle::Stone;
            nextGrid[(height - 2) * width + x] = Particle::Stone;
        }
        for (int y = 0; y < height - 1; y++) {
            nextGrid[y * width + 0] = Particle::Stone;
            nextGrid[y * width + (width - 1)] = Particle::Stone;
        }
    }

    void SetNext(int x, int y, Particle p) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        nextGrid[y * width + x] = p;
    }

    Color GetColorForParticle(Particle p) const {
        for (const auto& cm : colors) {
            if (cm.type == p) return cm.color;
        }
        return BLACK;
    }

    char GetSymbolForParticle(Particle p) const {
        for (const auto& cm : colors) {
            if (cm.type == p) return cm.symbol;
        }
        return ' ';
    }

    std::string GetParticleName(Particle p) const {
        for (const auto& cm : colors) {
            if (cm.type == p) return cm.name;
        }
        return "Unknown";
    }

    Particle ParticleFromSymbol(char c) const {
        for (const auto& cm : colors) {
            if (cm.symbol == c) return cm.type;
        }
        return Particle::Empty;
    }

    bool ProcessReactions(int x, int y) {
        Particle current = Get(x, y);
        
        // Reacciones químicas y térmicas
        switch (current) {
            case Particle::Lava:
                // Lava + Agua = Obsidiana + Vapor
                if (CheckNeighbor(x, y, Particle::Water)) {
                    SetNext(x, y, Particle::Obsidian);
                    ReplaceNeighbor(x, y, Particle::Water, Particle::Steam);
                    return true;
                }
                // Lava al enfriarse se convierte en piedra
                if (GetTemperature(x, y) < 500) {
                    SetNext(x, y, Particle::Stone);
                    return true;
                }
                break;
                
            case Particle::Water:
                // Agua + Lava/Fuego = Vapor
                if (CheckNeighbor(x, y, Particle::Lava) || CheckNeighbor(x, y, Particle::Fire) || GetTemperature(x, y) > 100) {
                    SetNext(x, y, Particle::Steam);
                    return true;
                }
                // Agua a baja temperatura = Hielo
                if (GetTemperature(x, y) < 0) {
                    SetNext(x, y, Particle::Ice);
                    return true;
                }
                break;
                
            case Particle::Ice:
                // Hielo + calor = Agua
                if (GetTemperature(x, y) > 0 || CheckNeighbor(x, y, Particle::Lava) || CheckNeighbor(x, y, Particle::Fire)) {
                    SetNext(x, y, Particle::Water);
                    return true;
                }
                break;
                
            case Particle::Steam:
                // Vapor se condensa en agua si está frío
                if (GetTemperature(x, y) < 50 && rand() % 20 == 0) {
                    SetNext(x, y, Particle::Water);
                    return true;
                }
                break;
                
            case Particle::Fire:
                // Fuego consume aceite
                if (CheckNeighbor(x, y, Particle::Oil)) {
                    ReplaceNeighbor(x, y, Particle::Oil, Particle::Fire);
                    // Fuego se apaga ocasionalmente
                    if (rand() % 30 == 0) {
                        SetNext(x, y, Particle::Smoke);
                        return true;
                    }
                }
                // Fuego + agua = vapor + se apaga
                else if (CheckNeighbor(x, y, Particle::Water)) {
                    SetNext(x, y, Particle::Smoke);
                    ReplaceNeighbor(x, y, Particle::Water, Particle::Steam);
                    return true;
                }
                // Fuego se apaga naturalmente
                else if (rand() % 50 == 0) {
                    SetNext(x, y, Particle::Smoke);
                    return true;
                }
                break;
                
            case Particle::Acid:
                // Ácido disuelve arena y piedra (excepto bordes)
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx > 0 && nx < width - 1 && ny > 0 && ny < height - 2) {
                            Particle neighbor = Get(nx, ny);
                            if (neighbor == Particle::Sand || neighbor == Particle::Ice) {
                                SetNext(nx, ny, Particle::Empty);
                                if (rand() % 8 == 0) {
                                    SetNext(x, y, Particle::Empty); // El ácido se consume
                                    return true;
                                }
                            }
                        }
                    }
                }
                break;
                
            case Particle::Oil:
                // Aceite + fuego = fuego
                if (CheckNeighbor(x, y, Particle::Fire) || CheckNeighbor(x, y, Particle::Lava)) {
                    SetNext(x, y, Particle::Fire);
                    return true;
                }
                break;
        }
        
        return false; // No hubo reacción
    }

    bool CheckNeighbor(int x, int y, Particle target) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                if (Get(x + dx, y + dy) == target) return true;
            }
        }
        return false;
    }

    void ReplaceNeighbor(int x, int y, Particle target, Particle replacement) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (Get(nx, ny) == target) {
                    SetNext(nx, ny, replacement);
                    return;
                }
            }
        }
    }

    bool CanMoveDown(int x, int y) {
        Particle below = Get(x, y + 1);
        Particle current = Get(x, y);
        
        if (below == Particle::Empty) return true;
        
        // Líquidos pueden atravesar vapor
        if (below == Particle::Steam && (current == Particle::Water || current == Particle::Lava || current == Particle::Acid || current == Particle::Oil)) {
            return true;
        }
        
        return false;
    }

    bool CanMoveSide(int x, int y, int dir) {
        Particle side = Get(x + dir, y);
        return side == Particle::Empty || side == Particle::Steam;
    }

    // Actualizaciones de partículas mejoradas
    void UpdateSand(int x, int y) {
        if (CanMoveDown(x, y)) {
            SetNext(x, y + 1, Particle::Sand);
            if (Get(x, y + 1) == Particle::Steam) {
                SetNext(x, y, Particle::Steam); // Intercambiar con vapor
            } else {
                SetNext(x, y, Particle::Empty);
            }
        } else {
            // Intentar moverse en diagonal
            int dir = (rand() % 2) ? 1 : -1;
            if (Get(x + dir, y + 1) == Particle::Empty || Get(x + dir, y + 1) == Particle::Steam) {
                SetNext(x + dir, y + 1, Particle::Sand);
                if (Get(x + dir, y + 1) == Particle::Steam) {
                    SetNext(x, y, Particle::Steam);
                } else {
                    SetNext(x, y, Particle::Empty);
                }
            } else if (Get(x - dir, y + 1) == Particle::Empty || Get(x - dir, y + 1) == Particle::Steam) {
                SetNext(x - dir, y + 1, Particle::Sand);
                if (Get(x - dir, y + 1) == Particle::Steam) {
                    SetNext(x, y, Particle::Steam);
                } else {
                    SetNext(x, y, Particle::Empty);
                }
            }
            // Si no puede moverse, se queda en su lugar (ya está copiado en nextGrid)
        }
    }

    void UpdateWater(int x, int y) {
        if (CanMoveDown(x, y)) {
            SetNext(x, y + 1, Particle::Water);
            if (Get(x, y + 1) == Particle::Steam) {
                SetNext(x, y, Particle::Steam);
            } else {
                SetNext(x, y, Particle::Empty);
            }
        } else {
            // Fluir horizontalmente
            int dir = (rand() % 2) ? 1 : -1;
            if (CanMoveSide(x, y, dir)) {
                SetNext(x + dir, y, Particle::Water);
                SetNext(x, y, Particle::Empty);
            } else if (CanMoveSide(x, y, -dir)) {
                SetNext(x - dir, y, Particle::Water);
                SetNext(x, y, Particle::Empty);
            }
            // Si no puede moverse horizontalmente, se queda
        }
    }

    void UpdateLava(int x, int y) {
        if (CanMoveDown(x, y)) {
            SetNext(x, y + 1, Particle::Lava);
            SetTemperature(x, y + 1, 1000);
            if (Get(x, y + 1) == Particle::Steam) {
                SetNext(x, y, Particle::Steam);
            } else {
                SetNext(x, y, Particle::Empty);
            }
        } else {
            int dir = (rand() % 2) ? 1 : -1;
            if (CanMoveSide(x, y, dir)) {
                SetNext(x + dir, y, Particle::Lava);
                SetTemperature(x + dir, y, 1000);
                SetNext(x, y, Particle::Empty);
            } else if (CanMoveSide(x, y, -dir)) {
                SetNext(x - dir, y, Particle::Lava);
                SetTemperature(x - dir, y, 1000);
                SetNext(x, y, Particle::Empty);
            }
        }
        // Mantener alta temperatura
        SetTemperature(x, y, 1000);
    }

    void UpdateSteam(int x, int y) {
        // El vapor sube
        if (Get(x, y - 1) == Particle::Empty) {
            SetNext(x, y - 1, Particle::Steam);
            SetNext(x, y, Particle::Empty);
        } else {
            // Moverse horizontalmente
            int dir = (rand() % 2) ? 1 : -1;
            if (Get(x + dir, y) == Particle::Empty) {
                SetNext(x + dir, y, Particle::Steam);
                SetNext(x, y, Particle::Empty);
            } else if (Get(x - dir, y) == Particle::Empty) {
                SetNext(x - dir, y, Particle::Steam);
                SetNext(x, y, Particle::Empty);
            }
            // El vapor se disipa lentamente
            else if (rand() % 100 == 0) {
                SetNext(x, y, Particle::Empty);
            }
        }
    }

    void UpdateIce(int x, int y) {
        // El hielo es sólido, no se mueve (ya está copiado)
    }

    void UpdateAcid(int x, int y) {
        // Similar al agua pero más agresivo
        if (CanMoveDown(x, y)) {
            SetNext(x, y + 1, Particle::Acid);
            if (Get(x, y + 1) == Particle::Steam) {
                SetNext(x, y, Particle::Steam);
            } else {
                SetNext(x, y, Particle::Empty);
            }
        } else {
            int dir = (rand() % 2) ? 1 : -1;
            if (CanMoveSide(x, y, dir)) {
                SetNext(x + dir, y, Particle::Acid);
                SetNext(x, y, Particle::Empty);
            } else if (CanMoveSide(x, y, -dir)) {
                SetNext(x - dir, y, Particle::Acid);
                SetNext(x, y, Particle::Empty);
            }
        }
    }

    void UpdateOil(int x, int y) {
        // Como agua pero más lento y menos denso
        if (rand() % 2 == 0) { // Movimiento más lento
            if (CanMoveDown(x, y)) {
                SetNext(x, y + 1, Particle::Oil);
                if (Get(x, y + 1) == Particle::Steam) {
                    SetNext(x, y, Particle::Steam);
                } else {
                    SetNext(x, y, Particle::Empty);
                }
            } else {
                int dir = (rand() % 2) ? 1 : -1;
                if (CanMoveSide(x, y, dir)) {
                    SetNext(x + dir, y, Particle::Oil);
                    SetNext(x, y, Particle::Empty);
                } else if (CanMoveSide(x, y, -dir)) {
                    SetNext(x - dir, y, Particle::Oil);
                    SetNext(x, y, Particle::Empty);
                }
            }
        }
    }

    void UpdateFire(int x, int y) {
        // El fuego tiende a subir y expandirse
        bool moved = false;
        
        if (Get(x, y - 1) == Particle::Empty && rand() % 3 == 0) {
            SetNext(x, y - 1, Particle::Fire);
            SetTemperature(x, y - 1, 800);
            moved = true;
        }
        
        // Expandirse a lados ocasionalmente
        if (!moved && rand() % 4 == 0) {
            int dir = (rand() % 2) ? 1 : -1;
            if (Get(x + dir, y) == Particle::Empty) {
                SetNext(x + dir, y, Particle::Fire);
                SetTemperature(x + dir, y, 800);
                moved = true;
            }
        }
        
        if (moved) {
            SetNext(x, y, Particle::Empty);
        }
        
        SetTemperature(x, y, 800);
    }

    void UpdateSmoke(int x, int y) {
        // El humo sube lentamente
        if (Get(x, y - 1) == Particle::Empty && rand() % 3 == 0) {
            SetNext(x, y - 1, Particle::Smoke);
            SetNext(x, y, Particle::Empty);
        } else {
            // Se dispersa
            int dir = (rand() % 3) - 1; // -1, 0, 1
            if (dir != 0 && Get(x + dir, y) == Particle::Empty && rand() % 4 == 0) {
                SetNext(x + dir, y, Particle::Smoke);
                SetNext(x, y, Particle::Empty);
            }
        }
        
        // El humo se desvanece gradualmente
        if (rand() % 80 == 0) {
            SetNext(x, y, Particle::Empty);
        }
    }
};

int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Enhanced Particle Simulator v2.0");
    SetTargetFPS(60);

    GameState gameState;
    ParticleGrid grid(GRID_WIDTH, GRID_HEIGHT, &gameState);
    
    // Intentar cargar mapa guardado al inicio
    grid.LoadFromFile("map.txt");

    Particle currentParticle = Particle::Sand;
    
    // Variables para smooth scrolling del mouse wheel
    float wheelMove = 0.0f;
    bool saveMessage = false;
    bool loadMessage = false;
    float messageTimer = 0.0f;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        // Actualizar transiciones de menú
        if (gameState.showMenu) {
            gameState.menuTransition = std::min(1.0f, gameState.menuTransition + deltaTime * 4.0f);
        } else {
            gameState.menuTransition = std::max(0.0f, gameState.menuTransition - deltaTime * 4.0f);
        }
        
        // Actualizar timer de mensajes
        if (messageTimer > 0) {
            messageTimer -= deltaTime;
            if (messageTimer <= 0) {
                saveMessage = false;
                loadMessage = false;
            }
        }

        // Input handling
        wheelMove = GetMouseWheelMove();
        if (wheelMove != 0 && !gameState.showMenu) {
            int currentSize = (int)gameState.brushSize;
            currentSize += (int)wheelMove * 2;
            currentSize = std::max(1, std::min(7, currentSize));
            gameState.brushSize = (BrushSize)currentSize;
        }

        // Particle selection (solo si no está el menú abierto)
        if (!gameState.showMenu) {
            if (IsKeyPressed(KEY_ONE)) currentParticle = Particle::Sand;
            if (IsKeyPressed(KEY_TWO)) currentParticle = Particle::Water;
            if (IsKeyPressed(KEY_THREE)) currentParticle = Particle::Lava;
            if (IsKeyPressed(KEY_FOUR)) currentParticle = Particle::Stone;
            if (IsKeyPressed(KEY_FIVE)) currentParticle = Particle::Steam;
            if (IsKeyPressed(KEY_SIX)) currentParticle = Particle::Ice;
            if (IsKeyPressed(KEY_SEVEN)) currentParticle = Particle::Acid;
            if (IsKeyPressed(KEY_EIGHT)) currentParticle = Particle::Oil;
            if (IsKeyPressed(KEY_NINE)) currentParticle = Particle::Fire;
            if (IsKeyPressed(KEY_ZERO)) currentParticle = Particle::Smoke;
        }

        // Game controls
        if (IsKeyPressed(KEY_SPACE)) gameState.isPaused = !gameState.isPaused;
        if (IsKeyPressed(KEY_R) && !gameState.showMenu) grid.Clear();
        if (IsKeyPressed(KEY_M)) gameState.showMenu = !gameState.showMenu;
        
        // Time speed control
        if (!gameState.showMenu) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                gameState.timeSpeed = std::min(4.0f, gameState.timeSpeed + 0.25f);
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                gameState.timeSpeed = std::max(0.1f, gameState.timeSpeed - 0.25f);
            }
        }

        // Save/Load
        if (!gameState.showMenu) {
            if (IsKeyPressed(KEY_G)) {
                grid.SaveToFile("map.txt");
                saveMessage = true;
                messageTimer = 2.0f;
            }
            if (IsKeyPressed(KEY_L)) {
                grid.LoadFromFile("map.txt");
                loadMessage = true;
                messageTimer = 2.0f;
            }
        }

        // Mouse input (solo si no está en menú)
        if (!gameState.showMenu) {
            Vector2 mousePos = GetMousePosition();
            int mouseX = (int)(mousePos.x / CELL_SIZE);
            int mouseY = (int)(mousePos.y / CELL_SIZE);
            
            if (mouseY < GRID_HEIGHT) { // Dentro del área de simulación
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    grid.AddParticle(mouseX, mouseY, currentParticle, gameState.brushSize);
                }
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                    grid.AddParticle(mouseX, mouseY, Particle::Empty, gameState.brushSize);
                }
            }
        }

        // Update simulation
        grid.Update();

        // Rendering
        BeginDrawing();
        ClearBackground(BLACK);

        // Dibujar simulación
        grid.Draw();
        grid.DrawUI(currentParticle, gameState.brushSize);
        
        // Mensajes de estado
        if (saveMessage) {
            DrawRectangle(WINDOW_WIDTH - 150, 10, 140, 30, { 0, 255, 0, 200 });
            DrawText("Saved!", WINDOW_WIDTH - 130, 20, 16, WHITE);
        }
        if (loadMessage) {
            DrawRectangle(WINDOW_WIDTH - 150, 10, 140, 30, { 0, 100, 255, 200 });
            DrawText("Loaded!", WINDOW_WIDTH - 130, 20, 16, WHITE);
        }
        
        // Dibujar menú con transición
        if (gameState.menuTransition > 0.0f) {
            // Aplicar alpha basado en la transición
            Color overlayColor = { 0, 0, 0, (unsigned char)(180 * gameState.menuTransition) };
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, overlayColor);
            
            if (gameState.menuTransition > 0.5f) {
                grid.DrawModernMenu();
            }
        }

        // FPS counter (esquina superior izquierda)
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 12, { 100, 255, 100, 200 });

        EndDrawing();
    }

    // Auto-save on exit
    grid.SaveToFile("map.txt");

    CloseWindow();
    return 0;
}