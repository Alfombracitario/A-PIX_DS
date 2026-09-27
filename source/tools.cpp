#include "formatsglobals.h"
#include "tools.h"

// copia en el stack una parte de la imagen
void copyFromSurfaceToStack()
{
    hasClipboard = true;

    stackXres = MIN(surfaceMaxExp-surf.z,surf.w);
    stackYres = MIN(surfaceMaxExp-surf.z,surf.h);

    int stackW = 1 << stackXres;
    int stackH = 1 << stackYres;
    int rowBytes = stackW << 1;//*2 por u16

    int baseOffset = surf.x +
                     (surf.y << surf.w);

    u16 *src = surface + baseOffset;
    u16 *dst = stack;

    int surfaceStride = surf.fw;

    // en este caso copiamos todo de una ya que los bits están alineados
    if (stackXres == surf.w)
    {
        memcpy(dst, src, rowBytes * stackH);
        return;
    }

    // Copia normal por filas
    for (int y = stackH; y--;)
    {
        memcpy(dst, src, rowBytes);

        dst += stackW;
        src += surfaceStride;
    }
}
void cutFromSurfaceToStack()
{
    // copia pero limpia un fragmento de la pantalla
    // en vez de optimizar esto, lo haré de la manera más simple posible lol
    copyFromSurfaceToStack();
    // limpiar la pantalla
    int blockSize = 1<<surfaceMaxExp>>surf.z;
    AVdrawRectangle(surface, surf.x, blockSize, surf.y, blockSize, 0, surf.w);
    accurate = true;
    updated = true;
}

void pasteFromStackToSurface()
{
    if (!hasClipboard)
    {
        return;
    }
    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres);// fila en el stack
        int _y = ((i + surf.y) << surf.w) + surf.x; // fila en surface con offset

        for (int j = 0; j < xsize; j++)
        {
            if(_y+j < surfaceSize){
                surface[_y + j] = stack[y + j];
            }
        }
    }
    accurate = true;
    updated = true;
}
void flipH()
{
    copyFromSurfaceToStack();

    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres);                                              // fila en el stack
        int _y = ((i + surf.y) << surf.w) + surf.x; // fila en surface con offset

        for (int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + (xsize - 1 - j)];
        }
    }
}

void flipV()
{
    copyFromSurfaceToStack();
    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (((ysize - 1) - i) << stackXres);                              // fila en el stack
        int _y = ((i + surf.y) << surf.w) + surf.x; // fila en surface con offset

        for (int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + j];
        }
    }
}

void scaleUp()
{
    copyFromSurfaceToStack();

    int stackW = 1 << stackXres;
    int stackH = 1 << stackYres;

    // felicidades, encontraste la peor línea que verás en tu vida!
    if ((((((stackH - 1) << 1) + 1) + surf.y) << surf.w) + ((stackW - 1) << 1) + surf.x > (surf.fw << surf.h))
    {
        return;
    } // fuera de rango

    for (int sy = 0; sy < stackH; ++sy)
    {
        int srcBase = sy * stackW;

        // dos filas destino correspondientes a esta fila fuente
        int dstRow0 = ((sy << 1) + surf.y) << surf.w;
        int dstRow1 = (((sy << 1) + 1) + surf.y) << surf.w;

        for (int sx = 0; sx < stackW; ++sx)
        {
            u16 pix = stack[srcBase + sx];
            int dstCol = (sx << 1) + surf.x;

            // escribir 2x2
            surface[dstRow0 + dstCol] = pix;
            surface[dstRow0 + dstCol + 1] = pix;
            surface[dstRow1 + dstCol] = pix;
            surface[dstRow1 + dstCol + 1] = pix;
        }
    }
}

#define A_MASK 0x8000
#define R_MASK 0x7C00
#define G_MASK 0x03E0
#define B_MASK 0x001F

void scaleDown()
{
    cutFromSurfaceToStack();
    int baseOffset = surf.x + (surf.y << surf.w);
    int _y = 0;
    int offset = 0;
    if (paletteBpp != 16)
    {
        // lee el stack saltandose un pixel
        int yres = (1 << stackYres) >> 1;
        int xres = (1 << stackXres) >> 1;
        for (int y = 0; y < yres; y++)
        {
            // precalcular algunas cosas
            offset = (y << surf.w) + baseOffset;
            _y = (y << 1) << stackXres;

            for (int x = 0; x < xres; x++)
            { // dibujar
                surface[offset + x] = stack[_y + (x << 1)];
            }
        }
    }
    else
    {
        int yres = (1 << stackYres) >> 1;
        int xres = (1 << stackXres) >> 1;

        for (int y = 0; y < yres; y++)
        {
            offset = (y << surf.w) + baseOffset;

            int row0 = (y << 1) << stackXres;
            int row1 = row0 + (1 << stackXres);

            for (int x = 0; x < xres; x++)
            {
                int sx = x << 1;

                u16 p0 = stack[row0 + sx];
                u16 p1 = stack[row0 + sx + 1];
                u16 p2 = stack[row1 + sx];
                u16 p3 = stack[row1 + sx + 1];

                // extraer canales
                int r =
                    (((p0 & R_MASK) >> 10) +
                    ((p1 & R_MASK) >> 10) +
                    ((p2 & R_MASK) >> 10) +
                    ((p3 & R_MASK) >> 10))>>2;

                int g =
                    (((p0 & G_MASK) >> 5) +
                    ((p1 & G_MASK) >> 5) +
                    ((p2 & G_MASK) >> 5) +
                    ((p3 & G_MASK) >> 5))>>2;

                int b =
                    ((p0 & B_MASK) +
                    (p1 & B_MASK) +
                    (p2 & B_MASK) +
                    (p3 & B_MASK))>>2;

                // alpha: activo si alguno lo tiene
                u16 a = (p0 | p1 | p2 | p3) & A_MASK;

                surface[offset + x] =
                    a |
                    (r << 10) |
                    (g << 5) |
                    b;
            }
        }
    }
}

void rotatePositive()
{ // 90° Antihorario
    copyFromSurfaceToStack();

    int size = 1 << stackXres;
    int baseOffset = surf.x + (surf.y << surf.w);

    for (int y = 0; y < size; y++)
    {
        int destOffset = baseOffset + (y << surf.w);
        for (int x = 0; x < size; x++)
        {
            // (x, y) → (y, size-1-x)
            surface[destOffset + x] = stack[((size - 1 - x) << stackXres) + y];
        }
    }
}

void rotateNegative()
{ // 90° Horario
    copyFromSurfaceToStack();

    int size = 1 << stackXres;
    int baseOffset = surf.x + (surf.y << surf.w);

    // Para rotar horario, leer desde cuadrante inferior hacia la derecha
    for (int y = 0; y < size; y++)
    {
        int destOffset = baseOffset + (y << surf.w);
        for (int x = 0; x < size; x++)
        {
            // leer desde cuadrante rotado (x, y) → (size-1-y, x)
            surface[destOffset + x] = stack[(x << stackXres) + (size - 1 - y)];
        }
    }
}
void shiftDownWrap()
{
    copyFromSurfaceToStack();

    int width = 1 << stackXres;
    int height = 1 << stackYres;

    u16 temp[128];
    memcpy(temp, stack + ((height - 1) << stackXres), width * sizeof(u16));

    // Mover filas hacia abajo (esto NO toca la fila 0 todavía)
    for (int y = height - 1; y > 0; y--)
    {
        int current = y << stackXres;
        int prev = (y - 1) << stackXres;

        memcpy(stack + current, stack + prev, width * sizeof(u16));
    }

    // Recién ahora coloco la última fila guardada en la primera
    memcpy(stack, temp, width * sizeof(u16));

    pasteFromStackToSurface();
}

void shiftUpWrap()
{
    copyFromSurfaceToStack();

    const int width = 1 << stackXres;
    const int height = 1 << stackYres;

    u16 temp[128];
    memcpy(temp, stack, width * sizeof(u16));

    // Mover filas hacia arriba (esto NO toca la última fila todavía)
    for (int y = 0; y < height - 1; y++)
    {
        const int current = y << stackXres;
        const int next = (y + 1) << stackXres;

        memcpy(stack + current, stack + next, width * sizeof(u16));
    }
    //fila extra para evitar que desaparezca una por el desplazamientoS
    memcpy(stack + ((height - 1) << stackXres), temp, width * sizeof(u16));

    pasteFromStackToSurface();
}

void shiftRightWrap()
{
    copyFromSurfaceToStack();

    const int width = 1 << stackXres;
    const int height = 1 << stackYres;

    for (int y = 0; y < height; y++)
    {
        const int row = y << stackXres;

        const u16 last = stack[row + width - 1];

        for (int x = width - 1; x > 0; x--)
        {
            stack[row + x] = stack[row + x - 1];
        }

        stack[row] = last;
    }
    pasteFromStackToSurface();
}

void shiftLeftWrap()
{
    copyFromSurfaceToStack();

    int width = 1 << stackXres;
    int height = 1 << stackYres;

    for (int y = 0; y < height; y++)
    {
        int row = y << stackXres;

        u16 first = stack[row];

        for (int x = 0; x < width - 1; x++)
        {
            stack[row + x] = stack[row + x + 1];
        }

        stack[row + width - 1] = first;
    }

    pasteFromStackToSurface();
}
const char* getFileExtension(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot == NULL || dot == path) {
        return NULL;
    }
    return dot + 1;
}

static void replaceIndex(u16 *surface, u16 oldColor, u16 newColor)
{
    // guardar un backup para undo
    backupWrite();
    // ahora sí reemplazamos todos los indices
    int size = surf.fw << surf.h;
    for (int i = 0; i < size; i++)
    {
        if (surface[i] == oldColor)
            surface[i] = newColor;
    }
}

static void swapIndex(u16 oldIndex, u16 newIndex)
{
    // Swap en paleta
    u16 tmp = palette[oldIndex];
    palette[oldIndex] = palette[newIndex];
    palette[newIndex] = tmp;

    // Swap de índices en el surface
    int total = surf.fw << surf.h;
    for (int i = 0; i < total; i++)
    {
        if (surface[i] == oldIndex)
            surface[i] = newIndex;
        else if (surface[i] == newIndex)
            surface[i] = oldIndex;
    }
    // actualizamos ahora todo visualmente
    if (paletteBpp != 16)
    {
        drawSurfaceMain();
    }
    updatePal(0, &palettePos);
    drawColorPalette();
}
void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres)
{
    if (oldColor == newColor)
        return;
    if (bucketMode == 1)
    {
        replaceIndex(surface, oldColor, newColor);
        return;
    }
    else if (bucketMode == 2)
    {
        swapIndex(oldColor, newColor);
    }
    int width = 1 << xres;
    int height = 1 << yres;
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    if (surface[(y << xres) + x] != oldColor)
        return;

    // reinterpretar el stack global como bytes para doble capacidad
    u8 *stack8 = (u8 *)stack;
    int maxStack = (sizeof(stack) * 2);
    int sp = 0;

    stack8[sp++] = (u8)x;
    stack8[sp++] = (u8)y;

    while (sp > 0)
    {
        if (sp < 2)
            break; // seguridad mínima
        u8 cy = stack8[--sp];
        u8 cx = stack8[--sp];

        int idx = (cy << xres) + cx;
        if (surface[idx] != oldColor)
            continue;
        surface[idx] = newColor;

        // push vecinos con control de overflow
        if (sp <= maxStack - 8)
        {
            if (cx + 1 < width && surface[(cy << xres) + (cx + 1)] == oldColor)
            {
                stack8[sp++] = cx + 1;
                stack8[sp++] = cy;
            }
            if (cx > 0 && surface[(cy << xres) + (cx - 1)] == oldColor)
            {
                stack8[sp++] = cx - 1;
                stack8[sp++] = cy;
            }
            if (cy + 1 < height && surface[((cy + 1) << xres) + cx] == oldColor)
            {
                stack8[sp++] = cx;
                stack8[sp++] = cy + 1;
            }
            if (cy > 0 && surface[((cy - 1) << xres) + cx] == oldColor)
            {
                stack8[sp++] = cx;
                stack8[sp++] = cy - 1;
            }
        }
        else
        {
            break; // stack lleno → evita overflow
        }
    }
}