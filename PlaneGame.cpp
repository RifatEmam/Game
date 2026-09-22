#include <windows.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <string>

using namespace std;

const int W = 740;
const int H = 840;

struct Particle {
    float x, y;
    float vx, vy;
    int life, maxLife;
    COLORREF color;
};

struct Bullet {
    float x, y;
    float vx, vy;
    int damage;
    bool isLaser;
};

struct Bomb {
    float x, y;
    float vx, vy;
    int radius;
    COLORREF color;
};

struct Enemy {
    float x, y;
    int hp;
    float speed;
    int shootTime;
    int type;
};

struct Food {
    float x, y;
    int type;
    float floatOffset;
};

struct BossData {
    string name;
    COLORREF primaryColor;
    COLORREF eyeColor;
    COLORREF glowColor;
    int attackPattern;
};

vector<Bullet> bullets;
vector<Bomb> bombs;
vector<Enemy> enemies;
vector<Food> foods;
vector<Particle> particles;

// প্লেইন যাতে নিচে না লুকিয়ে থাকে, যথেষ্ট উপরে স্পন হবে
float playerX = W / 2.0f;
float playerY = H - 220.0f;
int lives = 3;
int roundNo = 1;
int score = 0;
int powerLevel = 1;

// Boss States
bool bossActive = false;
bool bossSpawnWarning = false;
int bossTimer = 0;
const int BOSS_DELAY_TICKS = 60 * 18;
float bossX = W / 2.0f;
float bossY = -120.0f;
int bossHP = 0;
int bossMaxHP = 0;
float bossVx = 3.0f;
BossData currentBoss;

int tickNo = 0;
int invincibleTicks = 0;
int screenShakeTicks = 0;

bool leftKey = false;
bool rightKey = false;
bool upKey = false;
bool downKey = false;
bool gameOver = false;
bool gameWon = false;
bool paused = false;
int shieldTicks = 0;
DWORD roundStart = 0;
DWORD pauseStarted = 0;
DWORD totalPaused = 0;

const char* alienNames[] = {
    "ARSHY", "ESHA", "MAHI", "ANWESHA", "OHONA",
    "SADIA", "NUSRA", "TASNIM", "NILA", "MIM",
    "TISHA", "NOVA", "RIYA", "ANIKA", "RUPA"
};

int RoundLimit() {
    return roundNo <= 5 ? 300 : (roundNo <= 10 ? 420 : 600);
}

int RemainingTime() {
    DWORD now = paused ? pauseStarted : GetTickCount();
    int elapsed = (int)((now - roundStart - totalPaused) / 1000);
    return max(0, RoundLimit() - elapsed);
}

BossData GetAlienBoss(int r) {
    BossData b;
    int type = (r - 1) % 4;
    if (type == 0) {
        b.name = "RIFAT EX - VOID STALKER";
        b.primaryColor = RGB(160, 40, 210);
        b.eyeColor = RGB(0, 255, 230);
        b.glowColor = RGB(220, 80, 255);
        b.attackPattern = 0;
    } else if (type == 1) {
        b.name = "RIFAT EX - CRIMSON BIO-TITAN";
        b.primaryColor = RGB(220, 30, 60);
        b.eyeColor = RGB(255, 235, 50);
        b.glowColor = RGB(255, 110, 50);
        b.attackPattern = 1;
    } else if (type == 2) {
        b.name = "RIFAT EX - NEON CYBER-HORROR";
        b.primaryColor = RGB(25, 175, 185);
        b.eyeColor = RGB(255, 40, 140);
        b.glowColor = RGB(80, 255, 220);
        b.attackPattern = 2;
    } else {
        b.name = "RIFAT EX - OMEGA APOCALYPSE";
        b.primaryColor = RGB(190, 160, 25);
        b.eyeColor = RGB(255, 40, 40);
        b.glowColor = RGB(255, 220, 90);
        b.attackPattern = 3;
    }
    b.name = string(alienNames[(r - 1) % 15]) + " - " + b.name;
    return b;
}

void SpawnExplosion(float x, float y, COLORREF color, int count = 22) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.x = x;
        p.y = y;
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float spd = 1.8f + (rand() % 45) / 10.0f;
        p.vx = cos(angle) * spd;
        p.vy = sin(angle) * spd;
        p.life = 0;
        p.maxLife = 15 + rand() % 20;
        p.color = color;
        particles.push_back(p);
    }
}

void StartRound() {
    bullets.clear();
    bombs.clear();
    enemies.clear();
    foods.clear();
    particles.clear();

    // প্লেইন পর্যাপ্ত ওপরে থাকবে
    playerX = W / 2.0f;
    playerY = H - 220.0f;
    lives = 3;
    invincibleTicks = 60;
    shieldTicks = 0;
    paused = false;
    roundStart = GetTickCount();
    totalPaused = 0;

    bossActive = false;
    bossSpawnWarning = false;
    bossTimer = 0;

    currentBoss = GetAlienBoss(roundNo);
    bossX = W / 2.0f;
    bossY = -120.0f;
    bossVx = 2.8f + (roundNo * 0.3f);

    bossMaxHP = 75 + (roundNo * 35);
    bossHP = bossMaxHP;

    tickNo = 0;
}

void RestartGame() {
    roundNo = 1;
    score = 0;
    lives = 3;
    powerLevel = 1;
    gameOver = false;
    gameWon = false;
    paused = false;
    StartRound();
}

void FillBox(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    RECT r = {x1, y1, x2, y2};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

void DrawTextAt(HDC dc, int x, int y, const char* msg, int size, COLORREF col, bool bold = true) {
    HFONT font = CreateFontA(
        size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Segoe UI"
    );
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, col);
    TextOutA(dc, x, y, msg, lstrlenA(msg));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void DamagePlayer() {
    if (invincibleTicks > 0 || shieldTicks > 0 || gameOver || gameWon) return;

    lives--;
    invincibleTicks = 80;
    screenShakeTicks = 15;
    SpawnExplosion(playerX, playerY, RGB(255, 60, 40), 35);

    if (lives <= 0) {
        lives = 0;
        gameOver = true;
    }
}

void FirePlayerWeapon() {
    if (powerLevel == 1) {
        bullets.push_back({playerX, playerY - 36, 0, -18.0f, 1, false});
    } else if (powerLevel == 2) {
        bullets.push_back({playerX - 16, playerY - 30, 0, -18.0f, 1, false});
        bullets.push_back({playerX + 16, playerY - 30, 0, -18.0f, 1, false});
    } else if (powerLevel == 3) {
        bullets.push_back({playerX, playerY - 40, 0, -19.0f, 2, true});
        bullets.push_back({playerX - 22, playerY - 24, -2.4f, -17.0f, 1, false});
        bullets.push_back({playerX + 22, playerY - 24, 2.4f, -17.0f, 1, false});
    } else {
        bullets.push_back({playerX - 10, playerY - 42, 0, -20.0f, 2, true});
        bullets.push_back({playerX + 10, playerY - 42, 0, -20.0f, 2, true});
        bullets.push_back({playerX - 30, playerY - 20, -4.0f, -17.0f, 1, false});
        bullets.push_back({playerX + 30, playerY - 20, 4.0f, -17.0f, 1, false});
    }
}

void DropFood(float x, float y) {
    Food food;
    food.x = x;
    food.y = y;
    food.type = rand() % 5;
    food.floatOffset = (rand() % 100) / 10.0f;
    foods.push_back(food);
}

void ExecuteBossAttack() {
    if (bossHP <= 0 || !bossActive) return;

    int pat = currentBoss.attackPattern;

    if (pat == 0) {
        for (int a = -45; a <= 45; a += 18) {
            float rad = (a + 90) * 3.14159f / 180.0f;
            bombs.push_back({bossX, bossY + 40, cos(rad) * 4.8f, sin(rad) * 4.8f, 7, currentBoss.glowColor});
        }
    } else if (pat == 1) {
        float dx = playerX - bossX;
        float dy = playerY - bossY;
        float dist = sqrt(dx * dx + dy * dy);
        if (dist > 0.1f) {
            bombs.push_back({bossX - 45, bossY + 30, (dx / dist) * 7.8f, (dy / dist) * 7.8f, 8, RGB(255, 50, 60)});
            bombs.push_back({bossX + 45, bossY + 30, (dx / dist) * 7.8f, (dy / dist) * 7.8f, 8, RGB(255, 50, 60)});
            bombs.push_back({bossX, bossY + 50, (dx / dist) * 8.2f, (dy / dist) * 8.2f, 9, currentBoss.eyeColor});
        }
    } else if (pat == 2) {
        for (int i = 0; i < 10; i++) {
            float rad = (i * 36) * 3.14159f / 180.0f;
            bombs.push_back({bossX, bossY + 25, cos(rad) * 4.3f, sin(rad) * 4.3f, 6, RGB(60, 255, 230)});
        }
    } else {
        for (int a = -40; a <= 40; a += 20) {
            float rad = (a + 90) * 3.14159f / 180.0f;
            bombs.push_back({bossX, bossY + 35, cos(rad) * 5.2f, sin(rad) * 5.2f, 7, currentBoss.glowColor});
        }
        float dx = playerX - bossX;
        float dy = playerY - bossY;
        float dist = sqrt(dx * dx + dy * dy);
        if (dist > 0.1f) {
            bombs.push_back({bossX, bossY + 45, (dx / dist) * 9.0f, (dy / dist) * 9.0f, 10, RGB(255, 240, 50)});
        }
    }
}

void UpdateGame() {
    if (gameOver || gameWon || paused) return;
    if (RemainingTime() <= 0) { gameOver = true; return; }

    tickNo++;
    if (shieldTicks > 0) shieldTicks--;
    if (invincibleTicks > 0) invincibleTicks--;
    if (screenShakeTicks > 0) screenShakeTicks--;

    if (!bossActive) {
        bossTimer++;
        if (bossTimer >= BOSS_DELAY_TICKS - 160 && bossTimer < BOSS_DELAY_TICKS) {
            bossSpawnWarning = true;
        }
        if (bossTimer >= BOSS_DELAY_TICKS) {
            bossActive = true;
            bossSpawnWarning = false;
        }
    }

    // Player Movement - Bounds Updated so it never touches bottom HUD
    float speed = 7.8f;
    if (leftKey) playerX -= speed;
    if (rightKey) playerX += speed;
    if (upKey) playerY -= speed;
    if (downKey) playerY += speed;

    playerX = max(45.0f, min((float)W - 45.0f, playerX));
    playerY = max(280.0f, min((float)H - 120.0f, playerY)); // নিচে যথেষ্ট স্পেস থাকবে

    // Thruster Boost
    if (tickNo % 2 == 0) {
        Particle p;
        p.x = playerX + (rand() % 14 - 7);
        p.y = playerY + 34;
        p.vx = (rand() % 20 - 10) / 10.0f;
        p.vy = 4.0f + (rand() % 30) / 10.0f;
        p.life = 0;
        p.maxLife = 12;
        p.color = RGB(0, 240, 255);
        particles.push_back(p);
    }

    // Auto-fire
    int fireInterval = (powerLevel >= 3) ? 6 : 8;
    if (tickNo % fireInterval == 0) {
        FirePlayerWeapon();
    }

    if (bossActive) {
        if (bossY < 135.0f) {
            bossY += 2.0f;
        } else {
            bossX += bossVx;
            if (bossX >= W - 140) {
                bossX = (float)(W - 140);
                bossVx = -abs(bossVx);
            }
            if (bossX <= 140) {
                bossX = 140.0f;
                bossVx = abs(bossVx);
            }

            int bossDelay = max(24, 75 - roundNo * 3);
            if (tickNo % bossDelay == 0) {
                ExecuteBossAttack();
            }
        }
    }

    int fleetInterval = max(26, 80 - roundNo * 3);
    if (tickNo % fleetInterval == 0) {
        Enemy e;
        e.x = (float)(40 + rand() % (W - 80));
        e.y = -30.0f;
        e.hp = 1 + roundNo / 4;
        e.speed = 2.8f + (roundNo * 0.2f);
        e.shootTime = 30 + rand() % 40;
        e.type = (rand() % 100 > 60) ? 1 : 0;
        enemies.push_back(e);
    }

    if (tickNo % 400 == 0) {
        DropFood((float)(40 + rand() % (W - 80)), -20.0f);
    }

    // Bullets Handling
    for (int i = (int)bullets.size() - 1; i >= 0; i--) {
        bullets[i].x += bullets[i].vx;
        bullets[i].y += bullets[i].vy;

        if (bullets[i].y < -30 || bullets[i].x < 0 || bullets[i].x > W) {
            bullets.erase(bullets.begin() + i);
            continue;
        }

        bool hit = false;

        if (bossActive && bossHP > 0 &&
            abs(bullets[i].x - bossX) < 85 &&
            abs(bullets[i].y - bossY) < 45) {
            bossHP -= bullets[i].damage;
            hit = true;
            score += 6;
            SpawnExplosion(bullets[i].x, bullets[i].y, currentBoss.eyeColor, 3);
        }

        if (!hit) {
            for (int j = (int)enemies.size() - 1; j >= 0; j--) {
                if (abs(bullets[i].x - enemies[j].x) < 28 &&
                    abs(bullets[i].y - enemies[j].y) < 24) {
                    enemies[j].hp -= bullets[i].damage;
                    hit = true;
                    SpawnExplosion(bullets[i].x, bullets[i].y, RGB(255, 170, 40), 3);

                    if (enemies[j].hp <= 0) {
                        score += 30;
                        SpawnExplosion(enemies[j].x, enemies[j].y, RGB(255, 70, 60), 16);
                        if (rand() % 4 == 0) DropFood(enemies[j].x, enemies[j].y);
                        enemies.erase(enemies.begin() + j);
                    }
                    break;
                }
            }
        }

        if (hit) bullets.erase(bullets.begin() + i);
    }

    if (bossActive && bossHP <= 0) {
        SpawnExplosion(bossX, bossY, currentBoss.glowColor, 90);
        score += roundNo * 400;
        if (roundNo >= 15) {
            gameWon = true;
        } else {
            roundNo++;
            StartRound();
        }
        return;
    }

    for (int i = (int)enemies.size() - 1; i >= 0; i--) {
        enemies[i].y += enemies[i].speed;
        enemies[i].shootTime--;

        if (enemies[i].shootTime <= 0 && enemies[i].y < playerY - 60) {
            bombs.push_back({enemies[i].x, enemies[i].y + 18, 0.0f, 5.8f + (roundNo * 0.2f), 5, RGB(255, 80, 80)});
            enemies[i].shootTime = 50 + rand() % 40;
        }

        if (abs(enemies[i].x - playerX) < 40 && abs(enemies[i].y - playerY) < 32) {
            DamagePlayer();
            SpawnExplosion(enemies[i].x, enemies[i].y, RGB(255, 80, 50), 16);
            enemies.erase(enemies.begin() + i);
            continue;
        }

        if (enemies[i].y > H + 40) enemies.erase(enemies.begin() + i);
    }

    for (int i = (int)bombs.size() - 1; i >= 0; i--) {
        bombs[i].x += bombs[i].vx;
        bombs[i].y += bombs[i].vy;

        if (abs(bombs[i].x - playerX) < (bombs[i].radius + 18) &&
            abs(bombs[i].y - playerY) < (bombs[i].radius + 20)) {
            DamagePlayer();
            bombs.erase(bombs.begin() + i);
            continue;
        }

        if (bombs[i].y > H + 40 || bombs[i].y < -40 || bombs[i].x < -30 || bombs[i].x > W + 30) {
            bombs.erase(bombs.begin() + i);
        }
    }

    for (int i = (int)foods.size() - 1; i >= 0; i--) {
        foods[i].y += 2.3f;
        foods[i].floatOffset += 0.08f;
        float actualX = foods[i].x + sin(foods[i].floatOffset) * 14.0f;

        if (abs(actualX - playerX) < 36 && abs(foods[i].y - playerY) < 36) {
            if (foods[i].type == 0) powerLevel = min(4, powerLevel + 1);
            else if (foods[i].type == 1) lives = min(3, lives + 1);
            else if (foods[i].type == 2) score += 150;
            else if (foods[i].type == 3) shieldTicks = 60 * 8;
            else {
                for (size_t q = 0; q < bombs.size(); q++)
                    SpawnExplosion(bombs[q].x, bombs[q].y, RGB(95, 240, 255), 3);
                bombs.clear();
            }

            SpawnExplosion(actualX, foods[i].y, RGB(255, 255, 140), 12);
            foods.erase(foods.begin() + i);
            continue;
        }

        if (foods[i].y > H + 30) foods.erase(foods.begin() + i);
    }

    for (int i = (int)particles.size() - 1; i >= 0; i--) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].life++;
        if (particles[i].life >= particles[i].maxLife) {
            particles.erase(particles.begin() + i);
        }
    }
}

void DrawAlienBoss(HDC dc) {
    if (!bossActive || bossHP <= 0) return;

    int bx = (int)bossX;
    int by = (int)bossY;

    POINT leftWing[] = {
        {bx - 20, by},
        {bx - 80, by - 35},
        {bx - 120, by - 15},
        {bx - 95, by + 25},
        {bx - 45, by + 35},
        {bx - 15, by + 20}
    };
    POINT rightWing[] = {
        {bx + 20, by},
        {bx + 80, by - 35},
        {bx + 120, by - 15},
        {bx + 95, by + 25},
        {bx + 45, by + 35},
        {bx + 15, by + 20}
    };

    HBRUSH wingBrush = CreateSolidBrush(currentBoss.primaryColor);
    HBRUSH oldB = (HBRUSH)SelectObject(dc, wingBrush);
    HPEN pen = CreatePen(PS_SOLID, 2, currentBoss.glowColor);
    HPEN oldP = (HPEN)SelectObject(dc, pen);

    Polygon(dc, leftWing, 6);
    Polygon(dc, rightWing, 6);

    for (int side = -1; side <= 1; side += 2) {
        POINT claw[] = {
            {bx + side * 40, by + 20},
            {bx + side * 60, by + 50},
            {bx + side * 30, by + 58},
            {bx + side * 25, by + 35}
        };
        Polygon(dc, claw, 4);
    }

    HBRUSH coreBrush = CreateSolidBrush(RGB(20, 24, 40));
    SelectObject(dc, coreBrush);
    Ellipse(dc, bx - 35, by - 30, bx + 35, by + 35);

    HBRUSH eyeBrush = CreateSolidBrush(currentBoss.eyeColor);
    SelectObject(dc, eyeBrush);
    Ellipse(dc, bx - 14, by - 12, bx + 14, by + 16);
    SelectObject(dc, oldB);
    DeleteObject(coreBrush);
    DeleteObject(eyeBrush);

    SelectObject(dc, oldP);
    DeleteObject(wingBrush);
    DeleteObject(pen);

    DrawTextAt(dc, bx - 54, by - 55, alienNames[(roundNo - 1) % 15], 16, currentBoss.glowColor);
}

// একদম ক্রিস্টাল ক্লিয়ার এবং উজ্জ্বল প্রিমিয়াম ফাইটার প্লেন
void DrawPlayerShip(HDC dc) {
    int px = (int)playerX;
    int py = (int)playerY;

    // ১. এক্সহস্ট ফায়ার
    FillBox(dc, px - 6, py + 26, px + 6, py + 38, RGB(255, 120, 20));
    FillBox(dc, px - 3, py + 32, px + 3, py + 44, RGB(255, 240, 70));

    // ২. মেইন উইংস (হালকা সায়ান ও নীল শেড)
    FillBox(dc, px - 46, py + 6, px + 46, py + 22, RGB(0, 210, 255));
    FillBox(dc, px - 52, py + 14, px + 52, py + 26, RGB(0, 140, 230));

    // ৩. ডুয়েল সাইড প্লাজমা গানস
    FillBox(dc, px - 48, py - 12, px - 42, py + 12, RGB(255, 60, 80));
    FillBox(dc, px + 42, py - 12, px + 48, py + 12, RGB(255, 60, 80));

    // ৪. মূল সেন্ট্রাল বডি
    FillBox(dc, px - 14, py - 32, px + 14, py + 26, RGB(20, 95, 230));
    FillBox(dc, px - 8, py - 40, px + 8, py - 28, RGB(80, 245, 255)); // ফ্রন্ট শার্প নোজ

    // ৫. নিয়ন ককপিট
    FillBox(dc, px - 6, py - 16, px + 6, py + 4, RGB(255, 250, 90));

    // ৬. নেইমপ্লেট
    DrawTextAt(dc, px - 52, py + 32, "Rifat The Boss", 15, RGB(180, 245, 255));

    // ৭. শিল্ড অরা
    if (shieldTicks > 0) {
        HPEN shield = CreatePen(PS_SOLID, 3, RGB(0, 255, 240));
        HPEN oldShield = (HPEN)SelectObject(dc, shield);
        HBRUSH oldFill = (HBRUSH)SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Ellipse(dc, px - 60, py - 52, px + 60, py + 52);
        SelectObject(dc, oldFill);
        SelectObject(dc, oldShield);
        DeleteObject(shield);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
        case WM_CREATE:
            srand((unsigned)time(NULL));
            StartRound();
            SetTimer(hwnd, 1, 16, NULL);
            return 0;

        case WM_KEYDOWN:
            if (wp == 'A' || wp == VK_LEFT)  leftKey = true;
            if (wp == 'D' || wp == VK_RIGHT) rightKey = true;
            if (wp == 'W' || wp == VK_UP)    upKey = true;
            if (wp == 'S' || wp == VK_DOWN)  downKey = true;
            if (wp == 'R' && (gameOver || gameWon)) RestartGame();
            if (wp == 'P' && !gameOver && !gameWon) {
                if (paused) {
                    totalPaused += GetTickCount() - pauseStarted;
                    paused = false;
                } else {
                    pauseStarted = GetTickCount();
                    paused = true;
                }
            }
            if (wp == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;

        case WM_KEYUP:
            if (wp == 'A' || wp == VK_LEFT)  leftKey = false;
            if (wp == 'D' || wp == VK_RIGHT) rightKey = false;
            if (wp == 'W' || wp == VK_UP)    upKey = false;
            if (wp == 'S' || wp == VK_DOWN)  downKey = false;
            return 0;

        case WM_KILLFOCUS:
            leftKey = rightKey = upKey = downKey = false;
            return 0;

        case WM_TIMER:
            UpdateGame();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC screen = BeginPaint(hwnd, &ps);

            HDC dc = CreateCompatibleDC(screen);
            HBITMAP bitmap = CreateCompatibleBitmap(screen, W, H);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(dc, bitmap);

            int offsetX = (screenShakeTicks > 0) ? (rand() % 9 - 4) : 0;
            int offsetY = (screenShakeTicks > 0) ? (rand() % 9 - 4) : 0;

            FillBox(dc, 0, 0, W, H, RGB(8, 11, 24));

            for (int i = 0; i < 90; i++) {
                int sx = (i * 157 + 23) % W;
                int sy = (i * 277 + tickNo * (i % 3 + 1)) % H;
                int sz = (i % 4 == 0) ? 2 : 1;
                FillBox(dc, sx, sy, sx + sz, sy + sz, RGB(90 + (i % 4) * 35, 130 + (i % 3) * 30, 200));
            }

            for (size_t i = 0; i < particles.size(); i++) {
                int px = (int)particles[i].x;
                int py = (int)particles[i].y;
                FillBox(dc, px, py, px + 2, py + 2, particles[i].color);
            }

            DrawAlienBoss(dc);

            for (size_t i = 0; i < enemies.size(); i++) {
                int ex = (int)enemies[i].x;
                int ey = (int)enemies[i].y;
                COLORREF eCol = (enemies[i].type == 1) ? RGB(255, 65, 120) : RGB(255, 140, 45);
                DrawTextAt(dc, ex - 22, ey - 36, alienNames[(roundNo + (int)i) % 15], 12, eCol);
                FillBox(dc, ex - 24, ey - 4, ex + 24, ey + 6, eCol);
                FillBox(dc, ex - 8, ey - 14, ex + 8, ey + 18, RGB(230, 235, 245));
            }

            for (size_t i = 0; i < bombs.size(); i++) {
                HBRUSH bBrush = CreateSolidBrush(bombs[i].color);
                HBRUSH oldB = (HBRUSH)SelectObject(dc, bBrush);
                int r = bombs[i].radius;
                Ellipse(dc, (int)bombs[i].x - r, (int)bombs[i].y - r, (int)bombs[i].x + r, (int)bombs[i].y + r);
                SelectObject(dc, oldB);
                DeleteObject(bBrush);
            }

            for (size_t i = 0; i < foods.size(); i++) {
                float actualX = foods[i].x + sin(foods[i].floatOffset) * 14.0f;
                COLORREF fCol = foods[i].type == 0 ? RGB(255, 215, 45) :
                    foods[i].type == 1 ? RGB(65, 245, 125) :
                    foods[i].type == 2 ? RGB(65, 195, 255) :
                    foods[i].type == 3 ? RGB(75, 240, 255) : RGB(255, 110, 160);
                const char* icon = foods[i].type == 0 ? "P" :
                    foods[i].type == 1 ? "+" : foods[i].type == 2 ? "$" :
                    foods[i].type == 3 ? "S" : "B";
                FillBox(dc, (int)actualX - 13, (int)foods[i].y - 13, (int)actualX + 13, (int)foods[i].y + 13, fCol);
                DrawTextAt(dc, (int)actualX - 5, (int)foods[i].y - 10, icon, 18, RGB(10, 15, 30));
            }

            // Laser Bullets
            for (size_t i = 0; i < bullets.size(); i++) {
                int bx = (int)bullets[i].x;
                int by = (int)bullets[i].y;
                COLORREF shotColor = bullets[i].isLaser ? RGB(0, 255, 230) : RGB(255, 230, 90);
                FillBox(dc, bx - 15, by - 14, bx + 16, by + 5, shotColor);
                DrawTextAt(dc, bx - 14, by - 13, "RIFAT", 10, RGB(12, 22, 40));
            }

            // প্লেন ড্র করা (সর্বদা সবার ওপরে থাকবে)
            DrawPlayerShip(dc);

            if (bossSpawnWarning && (tickNo % 20 < 12)) {
                FillBox(dc, 140, 220, W - 140, 280, RGB(45, 15, 25));
                DrawTextAt(dc, 185, 235, "WARNING: BOSS APPROACHING!", 24, RGB(255, 50, 60));
            }

            // Top HUD Bar
            FillBox(dc, 0, 0, W, 75, RGB(14, 18, 35));
            FillBox(dc, 0, 74, W, 76, RGB(35, 55, 90));

            char buf[128];
            wsprintfA(buf, "ROUND: %02d/15", roundNo);
            DrawTextAt(dc, 20, 12, buf, 18, RGB(100, 215, 255));

            wsprintfA(buf, "SCORE: %06d", score);
            DrawTextAt(dc, 20, 40, buf, 20, RGB(255, 255, 255));

            DrawTextAt(dc, 220, 15, "SHIPS:", 16, RGB(180, 190, 210));
            for (int k = 0; k < lives; k++) {
                FillBox(dc, 280 + (k * 22), 16, 295 + (k * 22), 32, RGB(255, 60, 90));
            }

            wsprintfA(buf, "POWER: LVL %d", powerLevel);
            DrawTextAt(dc, 220, 42, buf, 16, RGB(255, 215, 60));
            wsprintfA(buf, "TIME %02d:%02d", RemainingTime() / 60, RemainingTime() % 60);
            DrawTextAt(dc, 370, 42, buf, 16, RGB(120, 245, 185));

            int barMaxWidth = 220;
            if (bossActive) {
                int currentBar = (bossHP > 0) ? (bossHP * barMaxWidth) / bossMaxHP : 0;
                DrawTextAt(dc, W - 245, 14, "BOSS BIO-CORE", 14, RGB(255, 90, 120));
                FillBox(dc, W - 245, 36, W - 245 + barMaxWidth, 48, RGB(40, 20, 35));
                FillBox(dc, W - 245, 36, W - 245 + currentBar, 48, currentBoss.glowColor);
            } else {
                int spawnProgress = (bossTimer * barMaxWidth) / BOSS_DELAY_TICKS;
                DrawTextAt(dc, W - 245, 14, "INCOMING WAVE", 14, RGB(140, 180, 220));
                FillBox(dc, W - 245, 36, W - 245 + barMaxWidth, 48, RGB(25, 35, 55));
                FillBox(dc, W - 245, 36, W - 245 + spawnProgress, 48, RGB(80, 160, 255));
            }

            DrawTextAt(dc, 20, 82, currentBoss.name.c_str(), 18, currentBoss.glowColor);

            // Bottom Navigation Hint
            FillBox(dc, 0, H - 42, W, H, RGB(14, 18, 35));
            DrawTextAt(dc, 15, H - 34,
                       "WASD / ARROWS MOVE | AUTO FIRE | P PAUSE | ESC EXIT", 14, RGB(150, 195, 225));
            if (paused) {
                FillBox(dc, 210, 330, 530, 440, RGB(20, 29, 55));
                DrawTextAt(dc, 287, 345, "PAUSED", 34, RGB(110, 245, 245));
                DrawTextAt(dc, 262, 395, "Press P to resume", 20, RGB(230, 240, 255));
            }

            if (gameOver) {
                FillBox(dc, 120, 320, W - 120, 480, RGB(18, 22, 40));
                DrawTextAt(dc, 275, 345, "MISSION FAILED", 32, RGB(255, 60, 70));
                DrawTextAt(dc, 255, 410, "Press 'R' to Deploy Again", 20, RGB(220, 230, 240), false);
            }

            if (gameWon) {
                FillBox(dc, 100, 310, W - 100, 490, RGB(15, 35, 45));
                DrawTextAt(dc, 205, 340, "VICTORY ACHIEVED!", 34, RGB(80, 255, 160));
                DrawTextAt(dc, 230, 415, "All Rifat Ex Fleets Neutralized", 20, RGB(255, 255, 255), false);
            }

            BitBlt(screen, offsetX, offsetY, W, H, dc, 0, 0, SRCCOPY);

            SelectObject(dc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, message, wp, lp);
}

int main() {
    HINSTANCE instance = GetModuleHandle(NULL);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = "RifatExAlienWars";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassA(&wc)) return 1;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT area = {0, 0, W, H};
    AdjustWindowRect(&area, style, FALSE);

    HWND hwnd = CreateWindowExA(
        0, "RifatExAlienWars",
        "Rifat The Boss - Alien Wars",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        area.right - area.left, area.bottom - area.top,
        NULL, NULL, instance, NULL
    );

    if (hwnd == NULL) return 1;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
