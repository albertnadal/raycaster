#include "raylib.h"
#include <math.h> 
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <float.h>
#include <assert.h>

#define KEY_UP 265
#define KEY_DOWN 264
#define KEY_LEFT 263
#define KEY_RIGHT 262

#define TWO_PI 2*PI

#define TILE_SIZE 48
#define MAP_NUM_ROWS 13
#define MAP_NUM_COLS 20

#define MINIMAP_SCALE_FACTOR 0.50

#define WINDOW_WIDTH (MAP_NUM_COLS * TILE_SIZE)
#define WINDOW_HEIGHT (MAP_NUM_ROWS * TILE_SIZE)

#define TEXTURE_WIDTH 64
#define TEXTURE_HEIGHT 64

#define FOV_ANGLE (60 * (PI / 180))
#define DIST_PROJ_PLANE ((WINDOW_WIDTH / 2) / tan(FOV_ANGLE / 2))

#define NUM_RAYS WINDOW_WIDTH
#define MAX_WALLS_TRAVERSED_PER_RAY 10

#define FPS 100

#define TOTAL_PORTALS 2

const int map[MAP_NUM_ROWS][MAP_NUM_COLS] = {
    { 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 1, 0, 2, 0, 2, 0, 2, 0, 0, 0, 2, 0, 2, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
    { 1, 0, 0, 0, 2, 0, 0, 0, 2, 2, 0, 0, 0, 2, 1, 2, 2, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1 }
};

struct Player
{
    float x;
    float y;
    float width;
    float height;
    int turnDirection;	// -1 for left, +1 for right
    int walkDirection;	// -1 for back, +1 for front
    float rotationAngle;
    float walkSpeed;
    float turnSpeed;
    bool isCrossingPortal;
}
player;

struct WallHit
{
    int wallGridIndexX;
    int wallGridIndexY;
    float rayOriginX;
    float rayOriginY;
    float wallHitX;
    float wallHitY;
    float distance;
    bool wasHitVertical;
    int wallHitContent;
};

struct RayLight
{
    float rayAngle;
    int isRayFacingUp;
    int isRayFacingDown;
    int isRayFacingLeft;
    int isRayFacingRight;
    int wallsTraversedCount;
    struct WallHit walls[MAX_WALLS_TRAVERSED_PER_RAY];
}
rays[NUM_RAYS];

typedef struct
{
    int width;
    int height;
    uint32_t * texture_buffer;
}
texture_t;

typedef struct Portal
{
    int gridIndexX;
    int gridIndexY;
}
portal_t;

portal_t portals[TOTAL_PORTALS] = {
    { .gridIndexX = 1, .gridIndexY = 0 },
    { .gridIndexX = 18, .gridIndexY = 12 }
};

Image windowBuffer = { 0 };
Texture2D windowTexture = { 0 };

void ReleaseResources()
{
    UnloadImage(windowBuffer);	// Releases the RAM memory allocated for the window buffer data
    UnloadTexture(windowTexture);	// Releases the texture from the GPU memory
}

void Setup()
{
    player.x = WINDOW_WIDTH / 2;
    player.y = WINDOW_HEIGHT / 2;
    player.width = 1;
    player.height = 1;
    player.turnDirection = 0;
    player.walkDirection = 0;
    player.rotationAngle = PI;
    player.walkSpeed = 200;
    player.turnSpeed = 110 *(PI / 180);
    player.isCrossingPortal = false;

   	// Initialize the window buffer
    windowBuffer.width = WINDOW_WIDTH;
    windowBuffer.height = WINDOW_HEIGHT;
    windowBuffer.mipmaps = 1;
    windowBuffer.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

   	// allocate memory for the window buffer data
    unsigned int bufferSize = GetPixelDataSize(windowBuffer.width, windowBuffer.height, windowBuffer.format);
    windowBuffer.data = (uint32_t*) malloc(bufferSize);

   	// Initialize the window texture
    windowTexture = LoadTextureFromImage(windowBuffer);	// Creates and load the texture in the GPU
}

bool MapHasWallAt(float x, float y)
{
    if (x < 0 || x > WINDOW_WIDTH || y < 0 || y > WINDOW_HEIGHT)
    {
        return true;
    }
    int mapGridIndexX = floor(x / TILE_SIZE);
    int mapGridIndexY = floor(y / TILE_SIZE);
    return map[mapGridIndexY][mapGridIndexX] != 0;
}

int GetMapWallTypeAt(float x, float y)
{
    if (x < 0 || x > WINDOW_WIDTH || y < 0 || y > WINDOW_HEIGHT)
    {
        return 1;
    }
    int mapGridIndexX = floor(x / TILE_SIZE);
    int mapGridIndexY = floor(y / TILE_SIZE);
    return map[mapGridIndexY][mapGridIndexX];
}

portal_t* GetPortalAtGridPosition(int x, int y)
{
    for (int i=0; i < TOTAL_PORTALS; i++)
    {
        if ((portals[i].gridIndexX == x) && (portals[i].gridIndexY == y))
        {
            return &portals[i];
        }
    }

    // No portal found at the given position of the map grid
    return NULL;
}

portal_t* GetDestinationPortalFromSourcePortal(portal_t *sourcePortal)
{
    for (int i=0; i < TOTAL_PORTALS; i++)
    {
        // Return the other portal
        if (&portals[i] != sourcePortal)
        {
            return &portals[i];
        }
    }

    // No destination portal found
    return NULL;
}

void MovePlayer(float deltaTime)
{
    player.rotationAngle += player.turnDirection *player.turnSpeed * deltaTime;
    float moveStep = player.walkDirection *player.walkSpeed * deltaTime;

    float newPlayerX = player.x + cos(player.rotationAngle) *moveStep;
    float newPlayerY = player.y + sin(player.rotationAngle) *moveStep;

    int wallType = GetMapWallTypeAt(newPlayerX, newPlayerY);
    if (wallType == 0)
    {
        player.x = newPlayerX;
        player.y = newPlayerY;
        player.isCrossingPortal = false;
    }
    else if (player.isCrossingPortal && (wallType == 3))
    {
        player.x = newPlayerX;
        player.y = newPlayerY;
    }
    else if (!player.isCrossingPortal && (wallType == 3))
    {
        // Player is crossing a portal
        int portalGridIndexX = floor(newPlayerX / TILE_SIZE);
        int portalGridIndexY = floor(newPlayerY / TILE_SIZE);

        portal_t *sourcePortal = GetPortalAtGridPosition(portalGridIndexX, portalGridIndexY);
        assert (sourcePortal != NULL);
        portal_t *destinationPortal = GetDestinationPortalFromSourcePortal(sourcePortal);
        assert (destinationPortal != NULL);

        // Set the new position of the player in the destination portal
        player.x = (destinationPortal->gridIndexX * TILE_SIZE) + fmodf(newPlayerX, TILE_SIZE);
        player.y = ((destinationPortal->gridIndexY + 1) * TILE_SIZE) + sin(player.rotationAngle) *moveStep;;

        player.isCrossingPortal = true;
    }
}

void RenderPlayer()
{
    DrawRectangle(player.x *MINIMAP_SCALE_FACTOR,
        player.y *MINIMAP_SCALE_FACTOR,
        player.width *MINIMAP_SCALE_FACTOR,
        player.height *MINIMAP_SCALE_FACTOR,
        YELLOW);

    DrawLine(MINIMAP_SCALE_FACTOR *player.x,
        MINIMAP_SCALE_FACTOR *player.y,
        MINIMAP_SCALE_FACTOR *(player.x + cos(player.rotationAngle) *20),
        MINIMAP_SCALE_FACTOR *(player.y + sin(player.rotationAngle) *20),
        YELLOW);
}

float NormalizeAngle(float angle)
{
    angle = remainder(angle, TWO_PI);
    if (angle < 0)
    {
        angle = TWO_PI + angle;
    }
    return angle;
}

float distanceBetweenPoints(float x1, float y1, float x2, float y2, bool calculateSqrt)
{
    float dist = (x2 - x1) *(x2 - x1) + (y2 - y1) *(y2 - y1);
    return calculateSqrt ? sqrt(dist) : dist;
}

void CastRay(float rayAngle, int stripId)
{
    bool rayCastingFinished = false;
    int wallsTraversedCount = 0;
    rayAngle = NormalizeAngle(rayAngle);

    int isRayFacingDown = rayAngle > 0 && rayAngle < PI;
    int isRayFacingUp = !isRayFacingDown;

    int isRayFacingRight = rayAngle < 0.5 *PI || rayAngle > 1.5 * PI;
    int isRayFacingLeft = !isRayFacingRight;

    float x = player.x;
    float y = player.y;

   	// Find the y-coordinate of the closest horizontal grid intersection
    float yinterceptHorz = floor(y / TILE_SIZE) *TILE_SIZE;
    yinterceptHorz += isRayFacingDown ? TILE_SIZE : 0;

   	// Find the x-coordinate of the closest horizontal grid intersection
    float xinterceptHorz = x + (yinterceptHorz - y) / tan(rayAngle);

   	// Calculate the increment xstepHorz and ystepHorz
    float ystepHorz = TILE_SIZE;
    ystepHorz *= isRayFacingUp ? -1 : 1;

    float xstepHorz = TILE_SIZE / tan(rayAngle);
    xstepHorz *= (isRayFacingLeft && xstepHorz > 0) ? -1 : 1;
    xstepHorz *= (isRayFacingRight && xstepHorz < 0) ? -1 : 1;

    float nextHorzTouchX = xinterceptHorz;
    float nextHorzTouchY = yinterceptHorz;

   	// - - - - - - - - - -

   	// Find the x-coordinate of the closest vertical grid intersection
    float xinterceptVert = floor(x / TILE_SIZE) *TILE_SIZE;
    xinterceptVert += isRayFacingRight ? TILE_SIZE : 0;

   	// Find the y-coordinate of the closest vertical grid intersection
    float yinterceptVert = y + (xinterceptVert - x) *tan(rayAngle);

   	// Calculate the increment xstepVert and ystepVert
    float xstepVert = TILE_SIZE;
    xstepVert *= isRayFacingLeft ? -1 : 1;

    float ystepVert = TILE_SIZE* tan(rayAngle);
    ystepVert *= (isRayFacingUp && ystepVert > 0) ? -1 : 1;
    ystepVert *= (isRayFacingDown && ystepVert < 0) ? -1 : 1;

    float nextVertTouchX = xinterceptVert;
    float nextVertTouchY = yinterceptVert;

    bool rayIsCrossingPortal = false;
    float additionalDistance = 0.0f;

    while (!rayCastingFinished)
    {

       	// Check for horizontal ray-grid intersections
        bool foundHorzWallHit = false;
        float horzWallHitX = 0;
        float horzWallHitY = 0;
        int horzWallContent = 0;
        float horzXToCheck = 0;
        float horzYToCheck = 0;

       	// Increment xstepHorz and ystepHorz until we find a wall
        while (!foundHorzWallHit && nextHorzTouchX >= 0 && nextHorzTouchX <= WINDOW_WIDTH && nextHorzTouchY >= 0 && nextHorzTouchY <= WINDOW_HEIGHT)
        {
            horzXToCheck = nextHorzTouchX;
            horzYToCheck = nextHorzTouchY + (isRayFacingUp ? -1 : 0);

            if (MapHasWallAt(horzXToCheck, horzYToCheck))
            {
               	// found a wall hit
                horzWallHitX = nextHorzTouchX;
                horzWallHitY = nextHorzTouchY;
                horzWallContent = map[(int) floor(horzYToCheck / TILE_SIZE)][(int) floor(horzXToCheck / TILE_SIZE)];
                foundHorzWallHit = true;
                break;
            }

            nextHorzTouchX += xstepHorz;
            nextHorzTouchY += ystepHorz;
        }

       	// Check for vertical ray-grid intersections
        bool foundVertWallHit = false;
        float vertWallHitX = 0;
        float vertWallHitY = 0;
        int vertWallContent = 0;
        float vertXToCheck = 0;
        float vertYToCheck = 0;

       	// Increment xstepVert and ystepVert until we find a wall
        while (!foundVertWallHit && nextVertTouchX >= 0 && nextVertTouchX <= WINDOW_WIDTH && nextVertTouchY >= 0 && nextVertTouchY <= WINDOW_HEIGHT)
        {
            vertXToCheck = nextVertTouchX + (isRayFacingLeft ? -1 : 0);
            vertYToCheck = nextVertTouchY;

            if (MapHasWallAt(vertXToCheck, vertYToCheck))
            {
               	// found a wall hit
                vertWallHitX = nextVertTouchX;
                vertWallHitY = nextVertTouchY;
                vertWallContent = map[(int) floor(vertYToCheck / TILE_SIZE)][(int) floor(vertXToCheck / TILE_SIZE)];
                foundVertWallHit = true;
                break;
            }

            nextVertTouchX += xstepVert;
            nextVertTouchY += ystepVert;
        }

       	// Calculate both horizontal and vertical hit distances and choose the smallest one
        float horzHitDistance = foundHorzWallHit ?
            distanceBetweenPoints(x, y, horzWallHitX, horzWallHitY, true) :
            FLT_MAX;
        float vertHitDistance = foundVertWallHit ?
            distanceBetweenPoints(x, y, vertWallHitX, vertWallHitY, true) :
            FLT_MAX;

        if (vertHitDistance < horzHitDistance)
        {
            rays[stripId].walls[wallsTraversedCount].wallGridIndexX = (int) floor(vertXToCheck / TILE_SIZE);
            rays[stripId].walls[wallsTraversedCount].wallGridIndexY =  (int) floor(vertYToCheck / TILE_SIZE);
            rays[stripId].walls[wallsTraversedCount].distance = vertHitDistance + additionalDistance;
            rays[stripId].walls[wallsTraversedCount].wallHitX = vertWallHitX;
            rays[stripId].walls[wallsTraversedCount].wallHitY = vertWallHitY;
            rays[stripId].walls[wallsTraversedCount].wallHitContent = vertWallContent;
            rays[stripId].walls[wallsTraversedCount].wasHitVertical = true;

            nextVertTouchX += xstepVert;
            nextVertTouchY += ystepVert;
        }
        else
        {
            rays[stripId].walls[wallsTraversedCount].wallGridIndexX =  (int) floor(horzXToCheck / TILE_SIZE);
            rays[stripId].walls[wallsTraversedCount].wallGridIndexY = (int) floor(horzYToCheck / TILE_SIZE);
            rays[stripId].walls[wallsTraversedCount].distance = horzHitDistance + additionalDistance;
            rays[stripId].walls[wallsTraversedCount].wallHitX = horzWallHitX;
            rays[stripId].walls[wallsTraversedCount].wallHitY = horzWallHitY;
            rays[stripId].walls[wallsTraversedCount].wallHitContent = horzWallContent;
            rays[stripId].walls[wallsTraversedCount].wasHitVertical = false;

            nextHorzTouchX += xstepHorz;
            nextHorzTouchY += ystepHorz;
        }

        rays[stripId].walls[wallsTraversedCount].rayOriginX = x;
        rays[stripId].walls[wallsTraversedCount].rayOriginY = y;

        int wallHitType = rays[stripId].walls[wallsTraversedCount].wallHitContent;

        // Discard store the wall hit inside a portal tile
        if (rayIsCrossingPortal)
        {
            rayIsCrossingPortal = false;
            continue;
        }

        //if (stripId == 0)
        //{
        //    printf("(x:%f) (y:%f) (wallsTraversedCount:%d) (colission:%f,%f) (content:%d) (distance:%f)\n", x, y, wallsTraversedCount, rays[stripId].walls[wallsTraversedCount].wallHitX, rays[stripId].walls[wallsTraversedCount].wallHitY, wallHitType, rays[stripId].walls[wallsTraversedCount].distance);
        //}

       	// Walls of type 2 are translucid walls. Walls of type 3 are portals.
        if (((wallHitType != 2) && (wallHitType != 3)) || (wallsTraversedCount + 1 >= MAX_WALLS_TRAVERSED_PER_RAY))
        {
            rayCastingFinished = true;
        }
        else if (wallHitType == 3)
        {
            // A portal wall appeared in the viewport
            int wallGridIndexX = rays[stripId].walls[wallsTraversedCount].wallGridIndexX;
            int wallGridIndexY = rays[stripId].walls[wallsTraversedCount].wallGridIndexY;

            portal_t *sourcePortal = GetPortalAtGridPosition(wallGridIndexX, wallGridIndexY);
            assert (sourcePortal != NULL);
            portal_t *destinationPortal = GetDestinationPortalFromSourcePortal(sourcePortal);
            assert (destinationPortal != NULL);

            float wallHitX = rays[stripId].walls[wallsTraversedCount].wallHitX;
            float wallHitY = rays[stripId].walls[wallsTraversedCount].wallHitY;

            // Set the current position of the player in the destination portal
            x = (destinationPortal->gridIndexX * TILE_SIZE) + fmodf(wallHitX, TILE_SIZE);
            y = (destinationPortal->gridIndexY + 1) * TILE_SIZE;

            // Find the y-coordinate of the closest horizontal grid intersection
            yinterceptHorz = floor(y / TILE_SIZE) *TILE_SIZE;
            yinterceptHorz += isRayFacingDown ? TILE_SIZE : 0;

            // Find the x-coordinate of the closest horizontal grid intersection
            xinterceptHorz = x + (yinterceptHorz - y) / tan(rayAngle);

            nextHorzTouchX = xinterceptHorz;
            nextHorzTouchY = yinterceptHorz;

            // Find the x-coordinate of the closest vertical grid intersection
            xinterceptVert = floor(x / TILE_SIZE) *TILE_SIZE;
            xinterceptVert += isRayFacingRight ? TILE_SIZE : 0;

            // Find the y-coordinate of the closest vertical grid intersection
            yinterceptVert = y + (xinterceptVert - x) *tan(rayAngle);

            nextVertTouchX = xinterceptVert;
            nextVertTouchY = yinterceptVert;

            additionalDistance += rays[stripId].walls[wallsTraversedCount].distance;
            rayIsCrossingPortal = true;
        }

        wallsTraversedCount++;
    }

    rays[stripId].rayAngle = rayAngle;
    rays[stripId].wallsTraversedCount = wallsTraversedCount;
    rays[stripId].isRayFacingDown = isRayFacingDown;
    rays[stripId].isRayFacingUp = isRayFacingUp;
    rays[stripId].isRayFacingLeft = isRayFacingLeft;
    rays[stripId].isRayFacingRight = isRayFacingRight;
}

void CastAllRays()
{
    for (int col = 0; col < NUM_RAYS; col++)
    {
        float rayAngle = player.rotationAngle + atan((col - NUM_RAYS / 2) / DIST_PROJ_PLANE);
        CastRay(rayAngle, col);
    }
}

void RenderMap()
{
    for (int i = 0; i < MAP_NUM_ROWS; i++)
    {
        for (int j = 0; j < MAP_NUM_COLS; j++)
        {
            int tileX = j * TILE_SIZE;
            int tileY = i * TILE_SIZE;
            Color tileColor;

            switch (map[i][j])
            {
                case 0:
                    tileColor = (Color){ 200, 200, 200, 255 };
                    break;
                case 1:
                    tileColor = (Color){ 80, 80, 80, 255 };
                    break;
                case 2:
                    tileColor = (Color){ 0, 0, 200, 255 };
                    break;
                case 3:
                    tileColor = (Color){ 200, 0, 0, 255 };
                    break;
            }

            DrawRectangle(tileX *MINIMAP_SCALE_FACTOR,
                tileY *MINIMAP_SCALE_FACTOR,
                TILE_SIZE *MINIMAP_SCALE_FACTOR,
                TILE_SIZE *MINIMAP_SCALE_FACTOR,
                tileColor);
        }
    }
}

void RenderRays()
{
    for (int i = 0; i < NUM_RAYS; i++)
    {
        float startX = MINIMAP_SCALE_FACTOR *player.x;
        float startY = MINIMAP_SCALE_FACTOR *player.y;
        for (int j = 0; j < rays[i].wallsTraversedCount; j++)
        {
            float endX = MINIMAP_SCALE_FACTOR *rays[i].walls[j].wallHitX;
            float endY = MINIMAP_SCALE_FACTOR *rays[i].walls[j].wallHitY;
            DrawLine(startX,
                startY,
                endX,
                endY,
                j == 0 ? DARKGREEN : GREEN);

            if ((rays[i].walls[j].wallHitContent == 3) && (j+1 < rays[i].wallsTraversedCount))
            {
                startX = MINIMAP_SCALE_FACTOR * rays[i].walls[j+1].rayOriginX;
                startY = MINIMAP_SCALE_FACTOR * rays[i].walls[j+1].rayOriginY;
            }
            else
            {
                startX = endX;
                startY = endY;
            }
        }
    }
}

void ProcessInput()
{

    if (IsKeyReleased(KEY_UP) && player.walkDirection == +1)
    {
        player.walkDirection = 0;
    }
    if (IsKeyReleased(KEY_DOWN) && player.walkDirection == -1)
    {
        player.walkDirection = 0;
    }
    if (IsKeyReleased(KEY_RIGHT) && player.turnDirection == +1)
    {
        player.turnDirection = 0;
    }
    if (IsKeyReleased(KEY_LEFT) && player.turnDirection == -1)
    {
        player.turnDirection = 0;
    }

    if (IsKeyPressed(KEY_UP))
    {
        player.walkDirection = +1;
    }
    if (IsKeyPressed(KEY_DOWN))
    {
        player.walkDirection = -1;
    }
    if (IsKeyPressed(KEY_RIGHT))
    {
        player.turnDirection = +1;
    }
    if (IsKeyPressed(KEY_LEFT))
    {
        player.turnDirection = -1;
    }
}

void Update()
{
    float deltaTime = GetFrameTime();
    MovePlayer(deltaTime);
    CastAllRays();
}

uint32_t GetMixedColor(uint32_t colorA, uint32_t colorB)
{
    unsigned char blueA = colorA &(0xff);
    unsigned char greenA = (colorA >> 8) &(0xff);
    unsigned char redA = (colorA >> 16) &(0xff);
    unsigned char alphaA = (colorA >> 24) &(0xff);

    unsigned char blueB = colorB &(0xff);
    unsigned char greenB = (colorB >> 8) &(0xff);
    unsigned char redB = (colorB >> 16) &(0xff);
    unsigned char alphaB = (colorB >> 24) &(0xff);

    unsigned char blueResult = (blueA *alphaA / 255) + (blueB *alphaB *(255 - alphaA) / (255 *255));
    unsigned char greenResult = (greenA *alphaA / 255) + (greenB *alphaB *(255 - alphaA) / (255 *255));
    unsigned char redResult = (redA *alphaA / 255) + (redB *alphaB *(255 - alphaA) / (255 *255));
    unsigned char alphaResult = alphaA + (alphaB *(255 - alphaA) / 255);

    uint32_t result = ((uint32_t) 0 &0xFFFFFF00) | blueResult;
    result = (result & 0xFFFF00FF) | ((uint32_t) greenResult << 8);
    result = (result & 0xFF00FFFF) | ((uint32_t) redResult << 16);
    result = (result & 0x00FFFFFF) | ((uint32_t) alphaResult << 24);

    return result;
}

void SetPixelColorWindowBuffer(uint32_t color, int offset, bool makeOpaque)
{
    ((uint32_t*) windowBuffer.data)[offset] = makeOpaque ? (color | ((uint32_t) 255 << 24)) : color;
}

void MixPixelColorWindowBuffer(uint32_t color, int offset, bool makeOpaque)
{
    uint32_t mixedColor = GetMixedColor(color, ((uint32_t*) windowBuffer.data)[offset]);
    ((uint32_t*) windowBuffer.data)[offset] = makeOpaque ? (mixedColor | ((uint32_t) 255 << 24)) : mixedColor;
}

void Generate3DProjection()
{
    for (int i = 0; i < NUM_RAYS; i++)
    {
        bool isFarthestWall = true;

        for (int w = rays[i].wallsTraversedCount - 1; w >= 0; w--)
        {
            float perpDistance = rays[i].walls[w].distance* cos(rays[i].rayAngle - player.rotationAngle);
            float projectedWallHeight = (TILE_SIZE / perpDistance) *DIST_PROJ_PLANE;

            int wallStripHeight = (int) projectedWallHeight;

            int wallTopPixel = (WINDOW_HEIGHT / 2) - (wallStripHeight / 2);
            wallTopPixel = wallTopPixel < 0 ? 0 : wallTopPixel;

            int wallBottomPixel = (WINDOW_HEIGHT / 2) + (wallStripHeight / 2);
            wallBottomPixel = wallBottomPixel > WINDOW_HEIGHT ? WINDOW_HEIGHT : wallBottomPixel;

            bool rayHitPortal = rays[i].walls[w].wallHitContent == 3;
            int pixelAddr;

           	// set the color of the ceiling
            for (int y = 0; y < wallTopPixel; y++)
            {
                pixelAddr = (WINDOW_WIDTH *y) + i;
                SetPixelColorWindowBuffer(0xC8333333, pixelAddr, true);
            }

            if (!rayHitPortal)
            {
                // render the wall from wallTopPixel to wallBottomPixel
                for (int y = wallTopPixel; y < wallBottomPixel; y++)
                {
                    pixelAddr = (WINDOW_WIDTH *y) + i;
                    uint32_t wallPixelColor = rays[i].walls[w].wasHitVertical ? 0xC8FFFFFF : 0xC8CCCCCC;

                    if (isFarthestWall) SetPixelColorWindowBuffer(wallPixelColor, pixelAddr, w == 0);
                    else MixPixelColorWindowBuffer(wallPixelColor, pixelAddr, w == 0);
                }
            }

           	// set the color of the floor
            for (int y = wallBottomPixel; y < WINDOW_HEIGHT; y++)
            {
                pixelAddr = (WINDOW_WIDTH *y) + i;
                SetPixelColorWindowBuffer(0xC8777777, pixelAddr, true);
            }

            isFarthestWall = false;
        }
    }
}

void ClearWindowBuffer(uint32_t color)
{
    for (int x = 0; x < WINDOW_WIDTH; x++)
        for (int y = 0; y < WINDOW_HEIGHT; y++)
            ((uint32_t*) windowBuffer.data)[(WINDOW_WIDTH *y) + x] = color;
}

void RenderWindowBuffer()
{
   	// Update the texture in the GPU with the data of the window buffer
    UpdateTexture(windowTexture, windowBuffer.data);

   	// Send the order to the GPU to draw the texture
    DrawTexture(windowTexture, 0, 0, WHITE);
}

static void RenderFrame(void)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    Generate3DProjection();
    RenderWindowBuffer();
    ClearWindowBuffer(0xFF000000);
    RenderMap();
    RenderRays();
    RenderPlayer();
    DrawFPS(850, 10);
    EndDrawing();
}

int main(void)
{
    SetTraceLogLevel(4);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "ray caster v0.1.0");
   	SetTargetFPS(FPS);
    Setup();

    while (!WindowShouldClose())	// Detect window close button or ESC key
    {
        ProcessInput();
        Update();
        RenderFrame();
    }

    ReleaseResources();
    CloseWindow();
    return 0;
}