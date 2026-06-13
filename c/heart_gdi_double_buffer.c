#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

#define CLASS_NAME TEXT("HeartWindowClass")
#define WINDOW_NAME TEXT("Heart")

#define CANVAS_WIDTH 640
#define CANVAS_HEIGHT 480
#define FRAME_COUNT 40
#define HEART_POINT_CAPACITY 2000
#define INSIDE_POINT_CAPACITY 4000
#define RANDOM_HALO_COUNT 1000
#define MAX_HALO_PARTICLE_COUNT (RANDOM_HALO_COUNT + 7000)
#define FRAME_PARTICLE_CAPACITY \
    (MAX_HALO_PARTICLE_COUNT + HEART_POINT_CAPACITY + HEART_POINT_CAPACITY * 3 + INSIDE_POINT_CAPACITY)

static const double CanvasCenterX = CANVAS_WIDTH / 2.0;
static const double CanvasCenterY = CANVAS_HEIGHT / 2.0;
static const double ImageEnlarge = 8.0;
static const double Pi = 3.14159265358979323846;
static const COLORREF HeartColor = RGB(0xff, 0x71, 0x71);
static const COLORREF ShinyColor = RGB(0xff, 0xd6, 0xdd);

typedef struct _HEART_POINT {
    double X, Y;
} HEART_POINT;

typedef struct _HEART_INT_POINT {
    int X, Y;
} HEART_INT_POINT;

typedef struct _HEART {
    HEART_POINT Points[HEART_POINT_CAPACITY], ExtraPoints[HEART_POINT_CAPACITY * 3], Inside[INSIDE_POINT_CAPACITY];
    int PointCount, ExtraCount, InsideCount, RandomHalo;
} HEART;

typedef struct _BACK_BUFFER {
    HDC Dc;
    HBITMAP Bitmap, OldBitmap;
    int Width, Height;
} BACK_BUFFER;

typedef struct _DRAW_POINT {
    int X, Y, Size;
    COLORREF Color;
} DRAW_POINT;

typedef struct _DRAW_FRAME {
    DRAW_POINT Points[FRAME_PARTICLE_CAPACITY];
    int Count;
} DRAW_FRAME;

typedef struct _APP_BRUSHES {
    HBRUSH Heart, Shiny;
} APP_BRUSHES;

typedef struct _APP_CONTEXT {
    HEART Heart;
    DRAW_FRAME Frames[FRAME_COUNT];
    int CurrentFrame;
    APP_BRUSHES Brushes;
    BACK_BUFFER BackBuffer;
} APP_CONTEXT;

static APP_CONTEXT g_App;

static double RandUnit(void)
{
    return (rand() + 1.0) / (RAND_MAX + 2.0);
}

static int RandInt(int Low, int High)
{
    return Low + rand() % (High - Low + 1);
}

static int PointInCanvas(int X, int Y)
{
    return X >= 0 && X < CANVAS_WIDTH && Y >= 0 && Y < CANVAS_HEIGHT;
}

static HEART_INT_POINT HeartFunction(double T, double ShrinkRatio)
{
    double SinT = sin(T);
    double X = 16.0 * SinT * SinT * SinT;
    double Y = -(13.0 * cos(T) - 5.0 * cos(2.0 * T) -
                 2.0 * cos(3.0 * T) - cos(4.0 * T));
    HEART_INT_POINT Point;

    X *= ShrinkRatio;
    Y *= ShrinkRatio;

    X += CanvasCenterX;
    Y += CanvasCenterY;

    Point.X = (int)X;
    Point.Y = (int)Y;
    return Point;
}

static HEART_POINT ScatterInside(double X, double Y, double Beta)
{
    double RatioX = -Beta * log(RandUnit());
    double RatioY = -Beta * log(RandUnit());
    double Dx = RatioX * (X - CanvasCenterX);
    double Dy = RatioY * (Y - CanvasCenterY);
    HEART_POINT Point;

    Point.X = X - Dx;
    Point.Y = Y - Dy;
    return Point;
}

static HEART_POINT Shrink(double X, double Y, double Ratio)
{
    double Force = -1.0 / pow((X - CanvasCenterX) * (X - CanvasCenterX) +
                              (Y - CanvasCenterY) * (Y - CanvasCenterY),
                              0.6);
    double Dx = Ratio * Force * (X - CanvasCenterX);
    double Dy = Ratio * Force * (Y - CanvasCenterY);
    HEART_POINT Point;

    Point.X = X - Dx;
    Point.Y = Y - Dy;
    return Point;
}

static HEART_POINT CalcPosition(double X, double Y, double Ratio)
{
    double Force = 1.0 / pow((X - CanvasCenterX) * (X - CanvasCenterX) +
                             (Y - CanvasCenterY) * (Y - CanvasCenterY),
                             0.6);
    double Dx = Ratio * Force * (X - CanvasCenterX) + RandInt(-1, 1);
    double Dy = Ratio * Force * (Y - CanvasCenterY) + RandInt(-1, 1);
    HEART_POINT Point;

    Point.X = X - Dx;
    Point.Y = Y - Dy;
    return Point;
}

static HEART_POINT RandomizePosition(HEART_POINT Point, int Range)
{
    Point.X += RandInt(-Range, Range);
    Point.Y += RandInt(-Range, Range);
    return Point;
}

static void AddUniqueIntPoint(HEART_POINT *Points, int *Count, int MaxCount,
                              unsigned char *Marks, int X, int Y)
{
    int Index;

    if (!PointInCanvas(X, Y) || *Count >= MaxCount) {
        return;
    }

    Index = Y * CANVAS_WIDTH + X;
    if (Marks[Index]) {
        return;
    }

    Marks[Index] = 1;
    Points[*Count].X = (double)X;
    Points[*Count].Y = (double)Y;
    ++*Count;
}

static void BuildHeart(HEART *HeartData, int Number)
{
    unsigned char Marks[CANVAS_WIDTH * CANVAS_HEIGHT];
    int I;

    memset(HeartData, 0, sizeof(*HeartData));
    HeartData->RandomHalo = RANDOM_HALO_COUNT;

    memset(Marks, 0, sizeof(Marks));
    for (I = 0; I < Number; ++I) {
        HEART_INT_POINT Point = HeartFunction(RandUnit() * 2.0 * Pi, ImageEnlarge);
        AddUniqueIntPoint(HeartData->Points, &HeartData->PointCount,
                          HEART_POINT_CAPACITY, Marks, Point.X, Point.Y);
    }

    for (I = 0; I < HeartData->PointCount; ++I) {
        int J;

        for (J = 0; J < 3; ++J) {
            if (HeartData->ExtraCount < HEART_POINT_CAPACITY * 3) {
                HeartData->ExtraPoints[HeartData->ExtraCount++] =
                    ScatterInside(HeartData->Points[I].X,
                                  HeartData->Points[I].Y,
                                  0.05);
            }
        }
    }

    memset(Marks, 0, sizeof(Marks));
    for (I = 0; I < INSIDE_POINT_CAPACITY; ++I) {
        int Chosen = RandInt(0, HeartData->PointCount - 1);
        HEART_POINT Point = ScatterInside(HeartData->Points[Chosen].X,
                                          HeartData->Points[Chosen].Y,
                                          0.15);
        AddUniqueIntPoint(HeartData->Inside, &HeartData->InsideCount,
                          INSIDE_POINT_CAPACITY, Marks, (int)Point.X, (int)Point.Y);
    }
}

static void AddDrawPoint(DRAW_FRAME *Frame, double X, double Y, int Size, COLORREF Color)
{
    DRAW_POINT *Point;

    if (Frame->Count >= FRAME_PARTICLE_CAPACITY) {
        return;
    }

    Point = &Frame->Points[Frame->Count++];
    Point->X = (int)X;
    Point->Y = (int)Y;
    Point->Size = Size;
    Point->Color = Color;
}

static void RenderFrame(const HEART *HeartData, int FrameIndex, DRAW_FRAME *Frame)
{
    unsigned char HaloMarks[CANVAS_WIDTH * CANVAS_HEIGHT];
    double Wave = sin(2.0 * Pi * FrameIndex / FRAME_COUNT);
    double HaloWave = Wave * Wave;
    double Ratio = 10.0 * Wave;
    int HaloRadius = (int)(8.0 + 36.0 * HaloWave);
    int HaloNumber = (int)(3000.0 + 4000.0 * HaloWave), I;

    Frame->Count = 0;
    memset(HaloMarks, 0, sizeof(HaloMarks));

    for (I = 0; I < HaloNumber + HeartData->RandomHalo; ++I) {
        HEART_INT_POINT Base = HeartFunction(RandUnit() * 2.0 * Pi, ImageEnlarge);
        int Index;

        if (!PointInCanvas(Base.X, Base.Y)) {
            continue;
        }

        Index = Base.Y * CANVAS_WIDTH + Base.X;
        if (!HaloMarks[Index]) {
            HEART_POINT Point;
            int Size;

            HaloMarks[Index] = 1;
            Point = Shrink(Base.X, Base.Y, (double)HaloRadius);
            Point.X += RandInt(-14, 14);
            Point.Y += RandInt(-14, 14);
            Size = RandInt(0, 2) == 2 ? 2 : 1;
            AddDrawPoint(Frame, Point.X, Point.Y, Size, HeartColor);
        }
    }

    for (I = 0; I < HeartData->PointCount; ++I) {
        HEART_POINT Point = CalcPosition(HeartData->Points[I].X,
                                         HeartData->Points[I].Y,
                                         Ratio);
        AddDrawPoint(Frame, Point.X, Point.Y, RandInt(1, 3), HeartColor);
    }

    for (I = 0; I < HeartData->ExtraCount; ++I) {
        HEART_POINT Point = CalcPosition(HeartData->ExtraPoints[I].X,
                                         HeartData->ExtraPoints[I].Y,
                                         Ratio);
        Point = RandomizePosition(Point, 5);
        AddDrawPoint(Frame, Point.X, Point.Y, RandInt(1, 2), HeartColor);
    }

    for (I = 0; I < HeartData->InsideCount; ++I) {
        HEART_POINT Point = CalcPosition(HeartData->Inside[I].X,
                                         HeartData->Inside[I].Y,
                                         Ratio);
        COLORREF Color = RandInt(0, 2) == 2 ? ShinyColor : HeartColor;
        AddDrawPoint(Frame, Point.X, Point.Y, RandInt(1, 2), Color);
    }
}

static int BuildFrames(APP_CONTEXT *App)
{
    int Frame;

    srand((unsigned int)time(NULL));
    BuildHeart(&App->Heart, HEART_POINT_CAPACITY);

    for (Frame = 0; Frame < FRAME_COUNT; ++Frame) {
        RenderFrame(&App->Heart, Frame, &App->Frames[Frame]);
    }

    return 1;
}

static int CreateBrushes(APP_BRUSHES *Brushes)
{
    Brushes->Heart = CreateSolidBrush(HeartColor);
    if (!Brushes->Heart) {
        return 0;
    }

    Brushes->Shiny = CreateSolidBrush(ShinyColor);
    if (!Brushes->Shiny) {
        DeleteObject(Brushes->Heart);
        Brushes->Heart = NULL;
        return 0;
    }

    return 1;
}

static void FreeBrushes(APP_BRUSHES *Brushes)
{
    if (Brushes->Heart) {
        DeleteObject(Brushes->Heart);
        Brushes->Heart = NULL;
    }

    if (Brushes->Shiny) {
        DeleteObject(Brushes->Shiny);
        Brushes->Shiny = NULL;
    }
}

static void FreeBackBuffer(BACK_BUFFER *BackBuffer)
{
    if (BackBuffer->Dc) {
        if (BackBuffer->OldBitmap) {
            SelectObject(BackBuffer->Dc, BackBuffer->OldBitmap);
            BackBuffer->OldBitmap = NULL;
        }

        DeleteDC(BackBuffer->Dc);
        BackBuffer->Dc = NULL;
    }

    if (BackBuffer->Bitmap) {
        DeleteObject(BackBuffer->Bitmap);
        BackBuffer->Bitmap = NULL;
    }

    BackBuffer->Width = 0;
    BackBuffer->Height = 0;
}

static int EnsureBackBuffer(BACK_BUFFER *BackBuffer, HDC Hdc, int Width, int Height)
{
    if (Width <= 0 || Height <= 0) {
        return 0;
    }

    if (BackBuffer->Dc &&
        BackBuffer->Bitmap &&
        BackBuffer->Width == Width &&
        BackBuffer->Height == Height) {
        return 1;
    }

    FreeBackBuffer(BackBuffer);

    BackBuffer->Dc = CreateCompatibleDC(Hdc);
    if (!BackBuffer->Dc) {
        return 0;
    }

    BackBuffer->Bitmap = CreateCompatibleBitmap(Hdc, Width, Height);
    if (!BackBuffer->Bitmap) {
        FreeBackBuffer(BackBuffer);
        return 0;
    }

    BackBuffer->OldBitmap = (HBITMAP)SelectObject(BackBuffer->Dc, BackBuffer->Bitmap);
    if (!BackBuffer->OldBitmap) {
        FreeBackBuffer(BackBuffer);
        return 0;
    }

    BackBuffer->Width = Width;
    BackBuffer->Height = Height;
    return 1;
}

static void PaintFrame(APP_CONTEXT *App, HWND Hwnd, HDC Hdc)
{
    RECT Client;
    int ClientWidth, ClientHeight, X, Y;
    const DRAW_FRAME *Frame;
    int I;

    GetClientRect(Hwnd, &Client);

    ClientWidth = Client.right - Client.left;
    ClientHeight = Client.bottom - Client.top;

    if (!EnsureBackBuffer(&App->BackBuffer, Hdc, ClientWidth, ClientHeight)) {
        return;
    }

    X = (ClientWidth - CANVAS_WIDTH) / 2;
    Y = (ClientHeight - CANVAS_HEIGHT) / 2;
    Frame = &App->Frames[App->CurrentFrame];

    FillRect(App->BackBuffer.Dc, &Client, (HBRUSH)GetStockObject(BLACK_BRUSH));

    for (I = 0; I < Frame->Count; ++I) {
        const DRAW_POINT *Point = &Frame->Points[I];
        RECT Rect;
        HBRUSH Brush = Point->Color == ShinyColor ? App->Brushes.Shiny : App->Brushes.Heart;

        Rect.left = X + Point->X;
        Rect.top = Y + Point->Y;
        Rect.right = Rect.left + Point->Size;
        Rect.bottom = Rect.top + Point->Size;
        FillRect(App->BackBuffer.Dc, &Rect, Brush);
    }

    BitBlt(
        Hdc,
        0, 0, ClientWidth, ClientHeight,
        App->BackBuffer.Dc,
        0, 0,
        SRCCOPY
    );
}

static LRESULT CALLBACK WindowProc(HWND Hwnd, UINT Message,
                                   WPARAM WParam, LPARAM LParam)
{
    switch (Message) {
    case WM_CREATE:
        SetTimer(Hwnd, 1, 30, NULL);
        return 0;

    case WM_TIMER:
        if (WParam == 1) {
            g_App.CurrentFrame = (g_App.CurrentFrame + 1) % FRAME_COUNT;
            InvalidateRect(Hwnd, NULL, FALSE);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC Hdc = BeginPaint(Hwnd, &Paint);

        PaintFrame(&g_App, Hwnd, Hdc);
        EndPaint(Hwnd, &Paint);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(Hwnd, 1);
        FreeBackBuffer(&g_App.BackBuffer);
        FreeBrushes(&g_App.Brushes);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(Hwnd, Message, WParam, LParam);
}

int ErrorBox(LPCTSTR MessageText) {
    return MessageBox(
        NULL,
        MessageText,
        WINDOW_NAME,
        MB_OK | MB_ICONERROR
    );
}

int WINAPI MainImpl(HINSTANCE HInstance, int ShowCmd)
{
    const TCHAR ClassName[] = TEXT("HeartWindowClass");
    DWORD Style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    WNDCLASS WindowClass;
    RECT WindowRect;
    HWND Hwnd;
    MSG Message;

    if (!BuildFrames(&g_App)) {
        ErrorBox(TEXT("Failed to allocate animation frames."));
        return 1;
    }

    if (!CreateBrushes(&g_App.Brushes)) {
        ErrorBox(TEXT("Failed to create GDI brushes."));
        return 1;
    }

    memset(&WindowClass, 0, sizeof(WindowClass));
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = HInstance;
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindowClass.lpszClassName = ClassName;

    if (!RegisterClass(&WindowClass)) {
        FreeBackBuffer(&g_App.BackBuffer);
        FreeBrushes(&g_App.Brushes);
        ErrorBox(TEXT("Failed to register window class."));
        return 1;
    }

    WindowRect.left = 0;
    WindowRect.top = 0;
    WindowRect.right = CANVAS_WIDTH;
    WindowRect.bottom = CANVAS_HEIGHT;
    AdjustWindowRect(&WindowRect, Style, FALSE);

    Hwnd = CreateWindowEx(
        0,
        ClassName,
        WINDOW_NAME,
        Style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WindowRect.right - WindowRect.left,
        WindowRect.bottom - WindowRect.top,
        NULL,
        NULL,
        HInstance,
        NULL
    );
    if (!Hwnd) {
        FreeBackBuffer(&g_App.BackBuffer);
        FreeBrushes(&g_App.Brushes);
        ErrorBox(TEXT("Failed to create window."));
        return 1;
    }

    ShowWindow(Hwnd, ShowCmd);
    UpdateWindow(Hwnd);

    while (GetMessage(&Message, NULL, 0, 0) > 0) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return (int)Message.wParam;
}

int main()
{
    return MainImpl(GetModuleHandle(NULL), SW_SHOWDEFAULT);
}
