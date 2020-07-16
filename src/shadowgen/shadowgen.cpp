#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>
#include <IL/il.h>

const int ATLASW = 1024;
const int ATLASH = 1024;
const int SIZE = 128;
const int SIZEMEM = (SIZE * SIZE * 4);
const int ATLAS_PICPERROW = ATLASW / SIZE;
const int SPREAD_SIDE = 24;
const int SPREAD_UP = 16;
const int SPREAD_DOWN = 32;

#define FLIP(x) ((SIZE - 1) - (x))
#define TOBUF(x, y) ((x) + ((y) * SIZE))

#define MAKECOLOR(x) ((((dword)((x) * 255.f)) << 24) | 0xFF)

typedef unsigned char byte;
typedef unsigned __int32 dword;

inline float CalcSide(int n, int spread)
{
    if (n >= spread)
        return 0.f;

    return 1.f - ((float)n) / ((float)spread);
}

float CalcCorner(int x, int y, int spreadY, int spreadX)
{
    if (x > spreadX || y > spreadY)
        return 0.f;

    float sX = 1.f - CalcSide(x, spreadX);
    float sY = 1.f - CalcSide(y, spreadY);

    return 1.f - fmin(1.f, sqrt(sX * sX + sY * sY));// *sX * sY;
}

inline float fmax4(float* f)
{
    return fmax(fmax(f[0], f[1]), fmax(f[2], f[3]));
}

void ShadowCorner(dword* buffer, bool nw, bool ne, bool se, bool sw)
{
    float shadow[4];

    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            shadow[0] = nw ? CalcCorner(x, y, SPREAD_UP, SPREAD_SIDE) : 0.f;
            shadow[1] = ne ? CalcCorner(FLIP(x), y, SPREAD_UP, SPREAD_SIDE) : 0.f;
            shadow[2] = se ? CalcCorner(FLIP(x), FLIP(y), SPREAD_DOWN, SPREAD_SIDE) : 0.f;
            shadow[3] = sw ? CalcCorner(x, FLIP(y), SPREAD_DOWN, SPREAD_SIDE) : 0.f;

            float shadowf = fmax4(shadow);
            buffer[TOBUF(x, y)] |= MAKECOLOR(shadowf);
        }
    }
}

void ShadowSide(dword* buffer, bool n, bool e, bool s, bool w)
{
    float shadow[4];

    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            shadow[0] = w ? CalcSide(x, SPREAD_SIDE) : 0.f;
            shadow[1] = e ? CalcSide(FLIP(x), SPREAD_SIDE) : 0.f;
            shadow[2] = n ? CalcSide(y, SPREAD_UP) : 0.f;
            shadow[3] = s ? CalcSide(FLIP(y), SPREAD_DOWN) : 0.f;

            float shdist = sqrt(shadow[0] * shadow[0] + shadow[1] * shadow[1] + shadow[2] * shadow[2] + shadow[3] * shadow[3]);
            float shadowf = fmin(1.f, shdist);

            buffer[TOBUF(x, y)] |= MAKECOLOR(shadowf);
        }
    }
}

void SaveImage(dword* image, int width, int height, const char* file)
{
    static bool s_initDone = false;

    if (!s_initDone) {
        s_initDone = true;
        ilInit();
    }

    ILuint s_iImageID = 0;

    ilGenImages(1, &s_iImageID);
    ilBindImage(s_iImageID);
    ilEnable(IL_FILE_OVERWRITE);

    bool bSuccess = 0 != ilTexImage(width, height, 1, 4, IL_BGRA, IL_UNSIGNED_BYTE, image);

    if (!bSuccess) {
        return;
    }

    bool bRes = ilSaveImage(file) != 0;

    ilDeleteImages(1, &s_iImageID);
}

void AtlasAdd(dword* atlas, dword* image, int atlasidx)
{
    int xatlas = atlasidx % ATLAS_PICPERROW;
    int yatlas = (atlasidx / ATLAS_PICPERROW) * SIZE;
    int lastrow = (ATLASW - 1) * ATLASH;

    for (int y = 0; y < SIZE; y++) {
        int ofs = (lastrow - (yatlas + y) * ATLASW) + (xatlas* SIZE);
        memcpy(atlas + (ofs), image + (y * SIZE), SIZE * 4);
    }
}

void GenDebugImage(dword* buffer, int atlasidx)
{
    dword arr[] = { 0xFF0000FF, 0xFF00FFFF, 0xFF00FF00, 0xFFFFFF00, 0xFFFF0000, 0xFFFF00FF };
    dword col = arr[atlasidx % 6];

    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            buffer[TOBUF(x, y)] = col;
        }
    }
}

// This func should be syntax valid both in C++ and C#
int ComputeImageIndex(bool n, bool e, bool s, bool w, bool nw, bool ne, bool se, bool sw)
{
    // invalid combinations
    if ((nw && (n || w)) || (se && (s || e)) || (sw && (s || w)) || (ne && (n || e)))
        return -1;

    // 0 -> 14 : sides only
    if (!(nw || ne || se || sw))
        return ((n ? 1 : 0) | (e ? 2 : 0) | (s ? 4 : 0) | (w ? 8 : 0)) - 1;

    // 15 -> 29 : corners only
    if (!(n || e || s || w))
        return 14 + ((nw ? 1 : 0) | (ne ? 2 : 0) | (se ? 4 : 0) | (sw ? 8 : 0));

    // two corners and opposite side
    if (nw && ne && s)
        return 30;
    if (ne && se && w)
        return 31;
    if (sw && se && n)
        return 32;
    if (sw && nw && e)
        return 33;

    // corners with opposite sides
    if (nw && (s || e))
        return 33 + ((s ? 1 : 0) | (e ? 2 : 0)); // 34, 35 or 36
    if (ne && (s || w))
        return 36 + ((s ? 1 : 0) | (w ? 2 : 0)); // 37, 38 or 39
    if (se && (n || w))
        return 39 + ((n ? 1 : 0) | (w ? 2 : 0)); // 40, 41 or 42
    if (sw && (n || e))
        return 42 + ((n ? 1 : 0) | (e ? 2 : 0)); // 43, 44 or 45

    return -2;
}


int main(int argc, char* argv[])
{
    dword* buffer = (dword*)malloc(SIZEMEM);
    dword* atlas = (dword*)malloc(ATLASW * ATLASH * 4);
    char strbuf[2048];

    memset(atlas, 0, ATLASW * ATLASH * 4);

    //for (int i = 0; i < 16; i++) {
    //    ShadowSide(buffer, i & 1, i & 2, i & 4, i & 8);
    //    AtlasAdd(atlas, buffer, atlasidx++);
    //}

    int used[256];
    memset(used, 0, 256 * sizeof(int));

    for (int i = 0; i < 256; i++) {
        int atlasidx = ComputeImageIndex(i & 1, i & 2, i & 4, i & 8, i & 16, i & 32, i & 64, i & 128);

        if (atlasidx == -2)
            printf("Fallout for index = %d\n", i);

        if (atlasidx < 0)
            continue;

        if (used[atlasidx] != 0)
            printf("Conflict for ID = %d (%d and %d)\n", atlasidx, i, used[atlasidx]);

        sprintf(strbuf, "c:\\temp\\shadow_%03d.png", atlasidx);
        printf("%s...", strbuf);

        used[atlasidx] = i;

        memset(buffer, 0, SIZEMEM);
        ShadowSide(buffer, i & 1, i & 2, i & 4, i & 8);
        ShadowCorner(buffer, i & 16, i & 32, i & 64, i & 128);
        AtlasAdd(atlas, buffer, atlasidx);

        printf("done.\n");

        SaveImage(buffer, SIZE, SIZE, strbuf);
    }

    SaveImage(atlas, ATLASW, ATLASH, "c:\\temp\\atlas.png");

    printf("ALL done, press enter to close.\n");

    fgets(strbuf, 256, stdin);

	return 0;
}

