#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h> 

#define MAX_CHUNKS 1024
#define CHUNK_SIZE 16
#define VOXEL_COUNT (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

struct VoxelChunk {
    int sizeX, sizeY, sizeZ;
    uint8_t voxels[VOXEL_COUNT];
    bool initialized;
    int cx, cy, cz;
};

struct VoxelChunk chunkMap[MAX_CHUNKS];
int chunkCount = 0;

const char* chunkKey(int cx, int cy, int cz) {
    static char key[64];
    snprintf(key, sizeof(key), "%d,%d,%d", cx, cy, cz);
    return key;
}

struct VoxelChunk* findOrCreateChunk(int cx, int cy, int cz) {
    for (int i = 0; i < chunkCount; i++) {
        if (chunkMap[i].cx == cx && chunkMap[i].cy == cy && chunkMap[i].cz == cz) {
            return &chunkMap[i];
        }
    }
    if (chunkCount < MAX_CHUNKS) {
        struct VoxelChunk *chunk = &chunkMap[chunkCount++];
        memset(chunk->voxels, 0, sizeof(chunk->voxels));
        chunk->sizeX = CHUNK_SIZE;
        chunk->sizeY = CHUNK_SIZE;
        chunk->sizeZ = CHUNK_SIZE;
        chunk->cx = cx;
        chunk->cy = cy;
        chunk->cz = cz;
        chunk->initialized = true;
        return chunk;
    }
    return NULL;
}

void placeVoxel(int x, int y, int z) {
    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
    struct VoxelChunk *chunk = findOrCreateChunk(cx, cy, cz);
    if (chunk) {
        chunk->voxels[(x % CHUNK_SIZE) + (y % CHUNK_SIZE) * CHUNK_SIZE + (z % CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE] = 1;
    }
}

void removeVoxel(int x, int y, int z) {
    int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
    struct VoxelChunk *chunk = findOrCreateChunk(cx, cy, cz);
    if (chunk) {
        chunk->voxels[(x % CHUNK_SIZE) + (y % CHUNK_SIZE) * CHUNK_SIZE + (z % CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE] = 0;
    }
}

float playerPos[3] = {0, 10, 0};
float playerVel[3] = {0, 0, 0};
bool sandboxMode = true;

// New variables for health and ammo
int playerHealth = 100;
int playerAmmo = 30;

void simulatePhysics(float dt) {
    if (!sandboxMode) return;
    playerVel[1] -= 9.81f * dt;
    for (int i = 0; i < 3; i++) {
        playerPos[i] += playerVel[i] * dt;
    }
    if (playerPos[1] < 0) {
        playerPos[1] = 0;
        playerVel[1] = 0;
    }
}

SDL_Window* window = NULL;
SDL_Renderer* sdlRenderer = NULL;
SDL_Texture* fontTexture = NULL;  // Bitmap font texture
int selectedBlockType = 1;

// Function to load the bitmap font texture
SDL_Texture* loadTexture(const char* path) {
    SDL_Surface* loadedSurface = SDL_LoadBMP(path);
    if (loadedSurface == NULL) {
        printf("Unable to load image %s! SDL Error: %s\n", path, SDL_GetError());
        return NULL;
    }
    SDL_Texture* newTexture = SDL_CreateTextureFromSurface(sdlRenderer, loadedSurface);
    SDL_FreeSurface(loadedSurface);
    return newTexture;
}

// Function to draw text using bitmap font
void drawText(const char* text, int x, int y) {
    int charWidth = 8;  // Character width in the bitmap font
    int charHeight = 8; // Character height in the bitmap font
    for (const char* c = text; *c != '\0'; c++) {
        SDL_Rect srcRect = {(*c % 16) * charWidth, (*c / 16) * charHeight, charWidth, charHeight};
        SDL_Rect dstRect = {x, y, charWidth, charHeight};
        SDL_RenderCopy(sdlRenderer, fontTexture, &srcRect, &dstRect);
        x += charWidth;
    }
}

void drawHotbar() {
    SDL_SetRenderDrawColor(sdlRenderer, 50, 50, 50, 200);
    SDL_Rect bar = {50, 500, 700, 60};
    SDL_RenderFillRect(sdlRenderer, &bar);

    for (int i = 1; i <= 9; i++) {
        SDL_Rect slot = {50 + (i - 1) * 75, 505, 70, 50};
        if (i == selectedBlockType) {
            SDL_SetRenderDrawColor(sdlRenderer, 200, 200, 0, 255);
        } else {
            SDL_SetRenderDrawColor(sdlRenderer, 150, 150, 150, 255);
        }
        SDL_RenderFillRect(sdlRenderer, &slot);
    }
}

void drawCrosshair() {
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(sdlRenderer, 400, 290, 400, 310);
    SDL_RenderDrawLine(sdlRenderer, 390, 300, 410, 300);
}

void drawDebugInfo() {
    char info[256];
    snprintf(info, sizeof(info), "Pos: %.1f, %.1f, %.1f | Sandbox: %s", playerPos[0], playerPos[1], playerPos[2], sandboxMode ? "ON" : "OFF");
    drawText(info, 10, 10);
}

// New function to draw health HUD
void drawHealth() {
    char healthText[64];
    snprintf(healthText, sizeof(healthText), "Health: %d", playerHealth);
    drawText(healthText, 10, 40);
}

// New function to draw ammo HUD
void drawAmmo() {
    char ammoText[64];
    snprintf(ammoText, sizeof(ammoText), "Ammo: %d", playerAmmo);
    drawText(ammoText, 10, 70);
}

void renderUI() {
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
    drawHotbar();
    drawCrosshair();
    drawDebugInfo();
    drawHealth();  // Draw health HUD
    drawAmmo();    // Draw ammo HUD
    SDL_RenderPresent(sdlRenderer);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Voxel Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (sdlRenderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Load the bitmap font texture
    fontTexture = loadTexture("font.bmp");
    if (fontTexture == NULL) {
        printf("Failed to load font texture!\n");
        return 1;
    }

    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
        SDL_RenderClear(sdlRenderer);

        // Update game logic...

        renderUI();

        SDL_RenderPresent(sdlRenderer);
    }

    SDL_DestroyTexture(fontTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}