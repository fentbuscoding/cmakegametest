#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define MAX_CHUNKS 1024
#define CHUNK_SIZE 16
#define VOXEL_COUNT (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)
#define MAX_FRAMES_IN_FLIGHT 2

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
VkInstance vkInstance;
VkSurfaceKHR vkSurface;
TTF_Font* font = NULL;
int selectedBlockType = 1;

VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
VkDevice vkDevice;
VkQueue vkGraphicsQueue;
VkQueue vkPresentQueue;
VkSwapchainKHR vkSwapchain;
VkImage* vkSwapchainImages;
uint32_t vkSwapchainImageCount;
VkFormat vkSwapchainImageFormat;
VkExtent2D vkSwapchainExtent;
VkImageView* vkSwapchainImageViews;
VkRenderPass vkRenderPass;
VkPipelineLayout vkPipelineLayout;
VkPipeline vkGraphicsPipeline;
VkFramebuffer* vkSwapchainFramebuffers;
VkCommandPool vkCommandPool;
VkCommandBuffer* vkCommandBuffers;
VkSemaphore vkImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore vkRenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence vkInFlightFences[MAX_FRAMES_IN_FLIGHT];
size_t currentFrame = 0;

const char* validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};

const char* deviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

TTF_Font* loadFont(const char* path, int fontSize) {
    TTF_Font* font = TTF_OpenFont(path, fontSize);
    if (font == NULL) {
        printf("Failed to load font %s! SDL_ttf Error: %s\n", path, TTF_GetError());
    }
    return font;
}

void drawText(const char* text, int x, int y) {
    if (font == NULL) return;
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, textColor);
    if (textSurface != NULL) {
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(SDL_GetRenderer(window), textSurface);
        SDL_FreeSurface(textSurface);
        if (textTexture != NULL) {
            SDL_Rect renderQuad = {x, y, textSurface->w, textSurface->h};
            SDL_RenderCopy(SDL_GetRenderer(window), textTexture, NULL, &renderQuad);
            SDL_DestroyTexture(textTexture);
        }
    }
}

void drawHotbar() {
    SDL_SetRenderDrawColor(SDL_GetRenderer(window), 50, 50, 50, 200);
    SDL_Rect bar = {50, 500, 700, 60};
    SDL_RenderFillRect(SDL_GetRenderer(window), &bar);

    for (int i = 1; i <= 9; i++) {
        SDL_Rect slot = {50 + (i - 1) * 75, 505, 70, 50};
        if (i == selectedBlockType) {
            SDL_SetRenderDrawColor(SDL_GetRenderer(window), 200, 200, 0, 255);
        } else {
            SDL_SetRenderDrawColor(SDL_GetRenderer(window), 150, 150, 150, 255);
        }
        SDL_RenderFillRect(SDL_GetRenderer(window), &slot);
    }
}

void drawCrosshair() {
    SDL_SetRenderDrawColor(SDL_GetRenderer(window), 255, 255, 255, 255);
    SDL_RenderDrawLine(SDL_GetRenderer(window), 400, 290, 400, 310);
    SDL_RenderDrawLine(SDL_GetRenderer(window), 390, 300, 410, 300);
}

void drawDebugInfo() {
    char info[256];
    snprintf(info, sizeof(info), "Pos: %.1f, %.1f, %.1f | Sandbox: %s", playerPos[0], playerPos[1], playerPos[2], sandboxMode ? "ON" : "OFF");
    drawText(info, 10, 10);
}

void drawHealth() {
    char healthText[64];
    snprintf(healthText, sizeof(healthText), "Health: %d", playerHealth);
    drawText(healthText, 10, 40);
}

void drawAmmo() {
    char ammoText[64];
    snprintf(ammoText, sizeof(ammoText), "Ammo: %d", playerAmmo);
    drawText(ammoText, 10, 70);
}

void drawFPS(int fps) {
    char fpsText[32];
    snprintf(fpsText, sizeof(fpsText), "FPS: %d", fps);
    drawText(fpsText, 10, 100);
}

void renderUI(int fps) {
    SDL_SetRenderDrawBlendMode(SDL_GetRenderer(window), SDL_BLENDMODE_BLEND);
    drawHotbar();
    drawCrosshair();
    drawDebugInfo();
    drawHealth();
    drawAmmo();
    drawFPS(fps);
    SDL_RenderPresent(SDL_GetRenderer(window));
}

void handleInput(SDL_Event* e) {
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_w:
                playerPos[2] -= 1;
                break;
            case SDLK_s:
                playerPos[2] += 1;
                break;
            case SDLK_a:
                playerPos[0] -= 1;
                break;
            case SDLK_d:
                playerPos[0] += 1;
                break;
            case SDLK_SPACE:
                playerPos[1] += 1;
                break;
            case SDLK_LSHIFT:
                playerPos[1] -= 1;
                break;
        }
    }
}

// Vulkan setup functions

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    VkLayerProperties* availableLayers = (VkLayerProperties*)malloc(layerCount * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    for (size_t i = 0; i < sizeof(validationLayers) / sizeof(validationLayers[0]); i++) {
        bool layerFound = false;

        for (uint32_t j = 0; j < layerCount; j++) {
            if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            free(availableLayers);
            return false;
        }
    }

    free(availableLayers);
    return true;
}

void createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        printf("Validation layers requested, but not available!\n");
        exit(1);
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Voxel Game";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    unsigned int extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL)) {
        printf("Failed to get Vulkan instance extensions from SDL!\n");
        exit(1);
    }

    const char** extensionNames = (const char**)malloc(extensionCount * sizeof(const char*));
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames)) {
        printf("Failed to get Vulkan instance extensions from SDL!\n");
        exit(1);
    }

    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensionNames;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = sizeof(validationLayers) / sizeof(validationLayers[0]);
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, NULL, &vkInstance) != VK_SUCCESS) {
        printf("Failed to create Vulkan instance!\n");
        exit(1);
    }

    free(extensionNames);
}

typedef struct {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    bool isComplete;
} QueueFamilyIndices;

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices = {0};

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

    VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vkSurface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.graphicsFamily && indices.presentFamily) {
            indices.isComplete = true;
            break;
        }
    }

    free(queueFamilies);
    return indices;
}

void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance, &deviceCount, NULL);

    if (deviceCount == 0) {
        printf("Failed to find GPUs with Vulkan support!\n");
        exit(1);
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices);

    for (uint32_t i = 0; i < deviceCount; i++) {
        QueueFamilyIndices indices = findQueueFamilies(devices[i]);
        if (indices.isComplete) {
            vkPhysicalDevice = devices[i];
            break;
        } else {
            printf("Device %d does not have complete queue families\n", i);
        }
    }

    if (vkPhysicalDevice == VK_NULL_HANDLE) {
        printf("Failed to find a suitable GPU!\n");
        exit(1);
    }

    free(devices);
}

void createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(vkPhysicalDevice);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2];

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos[0] = queueCreateInfo;

    if (indices.graphicsFamily != indices.presentFamily) {
        VkDeviceQueueCreateInfo presentQueueCreateInfo = {};
        presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentQueueCreateInfo.queueFamilyIndex = indices.presentFamily;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos[1] = presentQueueCreateInfo;
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = indices.graphicsFamily != indices.presentFamily ? 2 : 1;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = sizeof(validationLayers) / sizeof(validationLayers[0]);
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(vkPhysicalDevice, &createInfo, NULL, &vkDevice) != VK_SUCCESS) {
        printf("Failed to create logical device!\n");
        exit(1);
    }

    vkGetDeviceQueue(vkDevice, indices.graphicsFamily, 0, &vkGraphicsQueue);
    vkGetDeviceQueue(vkDevice, indices.presentFamily, 0, &vkPresentQueue);
}

void initVulkan() {
    createInstance();
    if (!SDL_Vulkan_CreateSurface(window, vkInstance, &vkSurface)) {
        printf("Failed to create Vulkan surface!\n");
        exit(1);
    }
    pickPhysicalDevice();
    createLogicalDevice();
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Voxel Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_VULKAN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    font = loadFont("/home/tscomputer/Desktop/ccoding/OpenSans-Regular.ttf", 16);
    if (font == NULL) {
        printf("Failed to load font!\n");
        return 1;
    }

    initVulkan();

    bool quit = false;
    SDL_Event e;
    Uint32 startTime = SDL_GetTicks();
    int frameCount = 0;
    int fps = 0;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            handleInput(&e);
        }

        SDL_SetRenderDrawColor(SDL_GetRenderer(window), 0, 0, 0, 255);
        SDL_RenderClear(SDL_GetRenderer(window));

        simulatePhysics(0.016f);

        renderUI(fps);

        SDL_RenderPresent(SDL_GetRenderer(window));

        frameCount++;
        if (SDL_GetTicks() - startTime >= 1000) {
            fps = frameCount;
            frameCount = 0;
            startTime = SDL_GetTicks();
        }
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}