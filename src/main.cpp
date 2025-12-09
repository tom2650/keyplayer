// ============================================================================
// KeyPlayer - A lightweight C++ Audio Player
// Version 1.0.0: 增加播放列表交互 (鼠标悬停高亮、点击播放)
// ============================================================================

#define NOMINMAX 
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <shobjidl.h>   
#include <shlwapi.h>    
#include <algorithm> 
#include <vector>
#include <string> 
#include <cwctype>      
#include <fstream>
#include <sstream>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <imm.h>
#include <dwmapi.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib") 
#pragma comment(lib, "bass.lib")
#pragma comment(lib, "winmm.lib") 
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

#include "bass.h"
#include "resource.h"

// 菜单 ID
#define IDM_SET_PLAYLIST      1001 
#define IDM_SET_LYRICS        1004 
#define IDM_SET_SIMPLEMODE    1005 
#define IDM_SET_THEME         1006 

#define IDM_MODE_ORDER        2001 
#define IDM_MODE_LISTLOOP     2002 
#define IDM_MODE_SINGLELOOP   2003 

#define IDM_SET_COVER              3000
#define IDM_SET_SPECTRUM_BAR       3001 
#define IDM_SET_SPECTRUM_CIRC      3002 
#define IDM_SET_SPECTRUM_WAVE      3003
#define IDM_SET_SPECTRUM_BUBBLE    3004  
#define IDM_SET_SPECTRUM_GALAXY    3005  
#define IDM_SET_SPECTRUM_RADIAL    3006  
#define IDM_SET_SPECTRUM_BLOCKS    3007  
#define IDM_SET_SPECTRUM_FIRE      3008    
#define IDM_SET_SPECTRUM_CITY      3009    
#define IDM_SET_SPECTRUM_RIPPLE    3010  
#define IDM_SET_SPECTRUM_PIXEL     3011  
#define IDM_SET_SPECTRUM_LED       3012  
#define IDM_SET_SPECTRUM_RAIN      3013  
#define IDM_SET_SPECTRUM_NET       3014
#define IDM_SET_SPECTRUM_GRAVITY   3015
#define IDM_SET_SPECTRUM_SYNTHWAVE 3016
#define IDM_SET_SPECTRUM_SYMBIOTE  3017
#define IDM_SET_SPECTRUM_VINYL     3018
#define IDM_SET_SPECTRUM_SPIRAL    3019
#define IDM_SET_SPECTRUM_WARP      3020
#define IDM_SET_SPECTRUM_PLANET    3021
#define IDM_SET_SPECTRUM_GRIND     3022
#define IDM_SET_SPECTRUM_BLIZZARD  3023
#define IDM_SET_SPECTRUM_WINDOW    3024

#define IDT_TIMER_PLAYCHECK 1 
#define IDT_TIMER_UI        2 

enum PlayMode { PM_ORDER = 0, PM_LIST_LOOP, PM_SINGLE_LOOP };
enum SpectrumMode {
  SPEC_NONE = 0, SPEC_BAR = 1, SPEC_CIRCLE = 2, SPEC_WAVE = 3,
  SPEC_BUBBLE = 4, SPEC_GALAXY = 5, SPEC_RADIAL = 6, SPEC_BLOCKS = 7,
  SPEC_FIRE = 8, SPEC_CITY = 9, SPEC_RIPPLE = 10, SPEC_PIXEL = 11,
  SPEC_LED = 12, SPEC_RAIN = 13, SPEC_NET = 14, SPEC_GRAVITY = 15,
  SPEC_SYNTHWAVE = 16, SPEC_SYMBIOTE = 17, SPEC_VINYL = 18, SPEC_SPIRAL = 19,
  SPEC_WARP = 20, SPEC_PLANET = 21, SPEC_GRIND = 22, SPEC_BLIZZARD = 23,
  SPEC_WINDOW = 24
};

struct LyricEntry {
  double time;
  std::wstring text;  // 原文
  std::wstring trans; // 译文
  // 缓存该行歌词的显示高度 (40, 60, 或 80)
  float displayHeight = 40.0f;
  // 缓存原文占几行，译文占几行
  int origLines = 1;
  int transLines = 0;
};

// ============================================================================
// 辅助工具类
// ============================================================================
struct DpiHelper
{
  static void GetDpi(ID2D1RenderTarget* pRT, float& dpiX, float& dpiY) {
    if (pRT) pRT->GetDpi(&dpiX, &dpiY);
    else { dpiX = 96.0f; dpiY = 96.0f; }
  }

  static D2D1_POINT_2F PixelsToDips(POINT pt, ID2D1RenderTarget* pRT) {
    float dpiX, dpiY; GetDpi(pRT, dpiX, dpiY);
    return D2D1::Point2F(pt.x * 96.0f / dpiX, pt.y * 96.0f / dpiY);
  }

  static D2D1_POINT_2F ScreenToClientDips(HWND hwnd, ID2D1RenderTarget* pRT) {
    POINT pt; GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    return PixelsToDips(pt, pRT);
  }
};

D2D1_RECT_F GetAspectFitRect(D2D1_SIZE_F imgSize, D2D1_RECT_F boxRect) {
  float boxW = boxRect.right - boxRect.left;
  float boxH = boxRect.bottom - boxRect.top;
  if (imgSize.width == 0 || imgSize.height == 0 || boxW <= 0 || boxH <= 0) return boxRect;
  float scaleX = boxW / imgSize.width;
  float scaleY = boxH / imgSize.height;
  float scale = (scaleX < scaleY) ? scaleX : scaleY;
  float finalW = imgSize.width * scale;
  float finalH = imgSize.height * scale;
  float offsetX = boxRect.left + (boxW - finalW) / 2.0f;
  float offsetY = boxRect.top + (boxH - finalH) / 2.0f;
  return D2D1::RectF(offsetX, offsetY, offsetX + finalW, offsetY + finalH);
}

D2D1_COLOR_F HsvToRgb(float h, float s, float v) {
  float r, g, b;
  int i = (int)(h * 6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);
  switch (i % 6) {
  case 0: r = v, g = t, b = p; break;
  case 1: r = q, g = v, b = p; break;
  case 2: r = p, g = v, b = t; break;
  case 3: r = p, g = q, b = v; break;
  case 4: r = t, g = p, b = v; break;
  case 5: r = v, g = p, b = q; break;
  default: r = 0, g = 0, b = 0; break;
  }
  return D2D1::ColorF(r, g, b);
}

template <class T> void SafeRelease(T** ppT) { if (*ppT) { (*ppT)->Release(); *ppT = nullptr; } }

bool IsPointInRect(D2D1_POINT_2F pt, const D2D1_RECT_F& rc) {
  return pt.x >= rc.left && pt.x <= rc.right && pt.y >= rc.top && pt.y <= rc.bottom;
}

std::wstring FormatTime(double seconds) {
  int totalSec = (int)seconds;
  int m = totalSec / 60;
  int s = totalSec % 60;
  wchar_t buf[16];
  swprintf_s(buf, L"%02d:%02d", m, s);
  return std::wstring(buf);
}

// ============================================================================
// 全局资源与状态
// ============================================================================
HWND g_hwnd = nullptr;
ID2D1Factory* g_d2dFactory = nullptr;
ID2D1HwndRenderTarget* g_renderTarget = nullptr;
ID2D1SolidColorBrush* g_brush = nullptr;
ID2D1SolidColorBrush* g_brushHighlight = nullptr;
ID2D1SolidColorBrush* g_brushDim = nullptr;
ID2D1LinearGradientBrush* g_brushSpectrum = nullptr;

IDWriteFactory* g_dwriteFactory = nullptr;
IDWriteTextFormat* g_textFormat = nullptr;
IDWriteTextFormat* g_textFormatIcon = nullptr; // [新增] 图标专用字体
IDWriteTextFormat* g_textFormatCenter = nullptr;
IDWriteTextFormat* g_textFormatLyric = nullptr;
IDWriteTextFormat* g_textFormatSmall = nullptr;

IWICImagingFactory* g_wicFactory = nullptr;
ID2D1Bitmap* g_bitmap = nullptr;

D2D1_RECT_F g_imageBoxRect = D2D1::RectF(20.0f, 20.0f, 250.0f, 250.0f);
D2D1_RECT_F g_textBoxRect = D2D1::RectF(270.0f, 20.0f, 500.0f, 250.0f);
D2D1_RECT_F g_progressBarRect = D2D1::RectF(270.0f, 263.0f, 500.0f, 282.0f);

float g_scrollOffset = 0.0f;
float g_textTotalHeight = 0.0f;
float g_headerHeight = 0.0f;         // 头部信息的高度
const float LIST_ITEM_HEIGHT = 22.0f; // 列表每行的高度
std::chrono::steady_clock::time_point g_lastSysTime;
double g_smoothTime = 0.0;

bool  g_isPlaying = false;
bool  g_isSimpleMode = false;
bool  g_isDarkMode = false;
bool  g_showLyrics = false;
bool  g_isMinimized = false;  // 小化停止绘制

int   g_spectrumMode = SPEC_NONE;

HSTREAM                  g_stream = 0;
HPLUGIN                  g_hFlacPlugin = 0;
std::vector<std::wstring> g_playlist;
int                      g_currentIndex = -1;
int                      g_hoverIndex = -1; // 鼠标悬停的歌曲索引
float                    g_volume = 0.2f;
int                      g_playMode = PM_ORDER;

std::wstring             g_currentFilePath = L"";

std::vector<LyricEntry>  g_lyricsData;
std::wstring             g_lyricsDisplayStr = L"";
double                   g_musicTime = 0.0;

#define SPEC_BARS 64
float g_fftPeaks[SPEC_BARS] = { 0.0f };

struct AppButton { D2D1_RECT_F rect; bool isDown; bool isHover; const wchar_t* text; int id; };

// 频谱特效结构体
struct Bubble { float x, y, radius, speed, alpha; D2D1_COLOR_F color; };
std::vector<Bubble> g_bubbles;
struct StarParticle { float x, y, vx, vy, size, alpha; D2D1_COLOR_F color; };
std::vector<StarParticle> g_stars;
struct RippleRing { float radius, thickness, hue, alpha, speed; };
std::vector<RippleRing> g_ripples; DWORD g_lastRippleTime = 0;
struct RainDrop { float x, y, speed, length, alpha; bool isHeavy; D2D1_COLOR_F color; };
std::vector<RainDrop> g_rainDrops;
struct NetParticle { float x, y, vx, vy, baseSize; };
std::vector<NetParticle> g_particles;
// --- 引力奇点结构体 ---
struct GravParticle { float x, y; float vx, vy; float hue; float mass; };
const int GRAV_PARTICLE_COUNT = 800;
std::vector<GravParticle> g_gravParticles;
bool g_gravInit = false;
// --- 激光唱针结构体 ---
struct VinylParticle { float angle; float dist; float size; float alpha; D2D1_COLOR_F color; };
std::vector<VinylParticle> g_vinylParticles;
float g_vinylRotation = 0.0f;
// --- 时空螺旋结构体 ---
struct SpiralNode { float intensity; float hue; };
std::vector<SpiralNode> g_spiralHistory;
double g_lastSpiralTime = -1.0;
// --- 超光速隧道结构体 ---
struct WarpFrame { float z; float fft[32]; D2D1_COLOR_F color; };
std::vector<WarpFrame> g_warpTunnel;
bool g_warpInit = false;
// --- 行星系统结构体 ---
struct Planet { float angle; float distance; float speed; float size; D2D1_COLOR_F color; std::vector<D2D1_POINT_2F> trail; };
std::vector<Planet> g_planets;
bool g_planetInit = false;
// --- 宿命之环结构体 ---
struct AccelParticle { float x, y; float vx, vy; float life; float size; D2D1_COLOR_F color; };
std::vector<AccelParticle> g_accelParticles;
float g_circleRotation = 0.0f;
// --- 虚空磨盘结构体 ---
struct GrindSpark { float x, y; float vx, vy; float life; D2D1_COLOR_F color; };
std::vector<GrindSpark> g_grindSparks;
float g_innerAngle = 0.0f;
float g_outerAngle = 0.0f;
// --- 极地风暴粒子 ---
struct BlizzardParticle { float x, y; float vx, vy; float z; float life; };
const int BLIZZARD_COUNT = 600;
std::vector<BlizzardParticle> g_snowParticles;
bool g_blizzardInit = false;


AppButton g_buttons[4] = {
    { D2D1::RectF(20.0f,  260.0f, 70.0f,  285.0f), false, false, L"\uE892", 1 }, // 上一首图标
    { D2D1::RectF(80.0f,  260.0f, 130.0f, 285.0f), false, false, L"\uE768", 2 }, // 播放图标
    { D2D1::RectF(140.0f, 260.0f, 190.0f, 285.0f), false, false, L"\uE893", 3 }, // 下一首图标
    { D2D1::RectF(200.0f, 260.0f, 250.0f, 285.0f), false, false, L"\uE713", 4 }  // 设置图标
};

std::wstring g_mainTextStr =
L"极简音乐播放器\n\n"
L"沉浸体验\n"
L"- 纯净界面，专注于音乐与歌词\n"
L"- 实时频谱，感受每一拍的律动\n"
L"- 无损解码，还原真实声音细节\n\n"
L"开始探索\n"
L"V - 视觉    L - 歌词    M - 视图\n"
L"T - 主题    Space - 播放\n"
L"Arrows - 控制";
const wchar_t* g_currentText = g_mainTextStr.c_str();

// ============================================================================
// 函数声明
// ============================================================================
HRESULT CreateGraphicsResources();
void    DiscardGraphicsResources();
void    OnPaint();
void    OnResize(UINT width, UINT height);
HRESULT LoadBitmapFromFile(ID2D1RenderTarget* pRT, IWICImagingFactory* pWIC, PCWSTR uri, ID2D1Bitmap** ppBitmap);
std::wstring OpenFolderDialog(HWND hwnd);
void ScanFolderForAudio(const std::wstring& folderPath);
HRESULT LoadAlbumArtFromAudio(ID2D1RenderTarget* pRT, IWICImagingFactory* pWIC, PCWSTR filePath, ID2D1Bitmap** ppBitmap);
void TogglePlayback();
void PlayTrackByIndex(int index, bool startPlaying = true);
std::wstring GetTruncatedString(const std::wstring& text, int limitWeight);
void LoadAndParseLyrics(const std::wstring& audioPath);
void OpenAndPlayFile(const std::wstring& filePath);
void UpdateAppTitle(HWND hwnd);
void UpdateTitleBarTheme(HWND hwnd);
int CountLines(const std::wstring& text, float maxWidth, IDWriteTextFormat* pFormat, IDWriteFactory* pFactory);

// ============================================================================
// 窗口过程
// ============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_CREATE:
  {
    HIMC hImc = ImmGetContext(hwnd);
    if (hImc) { ImmAssociateContext(hwnd, NULL); ImmReleaseContext(hwnd, hImc); }
    UpdateTitleBarTheme(hwnd);
    return 0;
  }
  case WM_DPICHANGED:
  {
    float newDpiX = LOWORD(wParam), newDpiY = HIWORD(wParam);
    if (g_renderTarget) g_renderTarget->SetDpi(newDpiX, newDpiY);
    RECT* const prcNewWindow = (RECT*)lParam;
    SetWindowPos(hwnd, nullptr, prcNewWindow->left, prcNewWindow->top,
      prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top, SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  case WM_SIZE:
  {
    if (wParam == SIZE_MINIMIZED) {
      g_isMinimized = true;
      // 这里的 KillTimer(IDT_TIMER_UI) 可以保留，省电
      KillTimer(hwnd, IDT_TIMER_UI);
    }
    else {
      // 从最小化恢复
      if (g_isMinimized) {
        g_isMinimized = false;
        SetTimer(hwnd, IDT_TIMER_UI, 16, nullptr);

        // 恢复显示时，如果正在播放但没封面，重新加载一下资源 ---
        // 简单粗暴的方法：强制刷新一次 PlayTrackByIndex 的视觉部分
        // 但为了不打断音乐，只手动触发一次重绘，或者在这里重新加载封面
        // 最简单的补救：直接调用 PlayTrackByIndex(..., false) 会重置音乐，不行。

        // 正确做法：直接刷新重绘，OnPaint 里会处理一部分
        // 如果想完美，可以在这里判断 g_bitmap 是否为空，为空则重新加载
        if (g_renderTarget && !g_bitmap && !g_currentFilePath.empty()) {
          LoadAlbumArtFromAudio(g_renderTarget, g_wicFactory, g_currentFilePath.c_str(), &g_bitmap);
          if (!g_bitmap) LoadBitmapFromFile(g_renderTarget, g_wicFactory, L"cover.png", &g_bitmap);

          // 同时也得更新一下文字，防止文字没变
          std::wstring fileName = g_currentFilePath.substr(g_currentFilePath.find_last_of(L"\\") + 1);
          g_mainTextStr = L"正在播放 (" + std::to_wstring(g_currentIndex + 1) + L"/" + std::to_wstring(g_playlist.size()) + L"):\n" + fileName + L"\n\n播放列表";
        }
      }
      OnResize(LOWORD(lParam), HIWORD(lParam));
    }
    return 0;
  }
  case WM_TIMER:
  {
    if (wParam == IDT_TIMER_PLAYCHECK) {
      if (g_stream && g_isPlaying) {
        if (BASS_ChannelIsActive(g_stream) == BASS_ACTIVE_STOPPED) {
          if (g_playlist.empty()) return 0;
          if (g_playMode == PM_SINGLE_LOOP) PlayTrackByIndex(g_currentIndex);
          else if (g_playMode == PM_LIST_LOOP) { int next = g_currentIndex + 1; if (next >= (int)g_playlist.size()) next = 0; PlayTrackByIndex(next); }
          else { int next = g_currentIndex + 1; if (next < (int)g_playlist.size()) PlayTrackByIndex(next); else { g_isPlaying = false; g_buttons[1].text = L"\uE768"; BASS_ChannelSetPosition(g_stream, 0, BASS_POS_BYTE); InvalidateRect(hwnd, nullptr, FALSE); } }
        }
      }
    }
    else if (wParam == IDT_TIMER_UI) {
      if (g_isMinimized) return 0;
      if (g_stream && g_isPlaying) {
        QWORD pos = BASS_ChannelGetPosition(g_stream, BASS_POS_BYTE);
        g_musicTime = BASS_ChannelBytes2Seconds(g_stream, pos);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }
    return 0;
  }
  case WM_COMMAND:
  {
    int wmId = LOWORD(wParam);
    switch (wmId) {
    case IDM_SET_PLAYLIST: {
      std::wstring folder = OpenFolderDialog(hwnd);
      if (!folder.empty()) {
        ScanFolderForAudio(folder);
        if (!g_playlist.empty()) { g_currentIndex = 0; PlayTrackByIndex(0, false); }
        else {
          SafeRelease(&g_bitmap); LoadBitmapFromFile(g_renderTarget, g_wicFactory, L"cover.png", &g_bitmap);
          g_mainTextStr = L"目录为空或无音频文件：\n" + folder; g_currentText = g_mainTextStr.c_str(); InvalidateRect(hwnd, nullptr, FALSE);
        }
      }
    } break;
    case IDM_MODE_ORDER: g_playMode = PM_ORDER; break;
    case IDM_MODE_LISTLOOP: g_playMode = PM_LIST_LOOP; break;
    case IDM_MODE_SINGLELOOP: g_playMode = PM_SINGLE_LOOP; break;
    case IDM_SET_SIMPLEMODE: {
      g_isSimpleMode = !g_isSimpleMode;
      int newLogicalW = g_isSimpleMode ? 270 : 520; int newLogicalH = 305;
      HDC hdc = GetDC(hwnd); int dpiX = GetDeviceCaps(hdc, LOGPIXELSX), dpiY = GetDeviceCaps(hdc, LOGPIXELSY); ReleaseDC(hwnd, hdc);
      int physW = MulDiv(newLogicalW, dpiX, 96), physH = MulDiv(newLogicalH, dpiY, 96);
      DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE); RECT rc = { 0, 0, physW, physH }; AdjustWindowRect(&rc, dwStyle, FALSE);
      SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      UpdateAppTitle(hwnd);
      InvalidateRect(hwnd, nullptr, FALSE);
    } break;
    case IDM_SET_COVER: g_spectrumMode = SPEC_NONE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_BAR: g_spectrumMode = SPEC_BAR; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_CIRC: g_spectrumMode = SPEC_CIRCLE; g_accelParticles.clear(); InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_WAVE: g_spectrumMode = SPEC_WAVE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_BUBBLE: g_spectrumMode = SPEC_BUBBLE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_GALAXY: g_spectrumMode = SPEC_GALAXY; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_RADIAL: g_spectrumMode = SPEC_RADIAL; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_BLOCKS: g_spectrumMode = SPEC_BLOCKS; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_FIRE: g_spectrumMode = SPEC_FIRE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_CITY: g_spectrumMode = SPEC_CITY; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_RIPPLE: g_spectrumMode = SPEC_RIPPLE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_PIXEL: g_spectrumMode = SPEC_PIXEL; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_LED: g_spectrumMode = SPEC_LED; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_RAIN: g_spectrumMode = SPEC_RAIN; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_NET: g_spectrumMode = SPEC_NET; g_particles.clear(); for (int i = 0; i < 60; i++) { NetParticle p; p.x = rand() % 500; p.y = rand() % 250; p.vx = (rand() % 200) / 100.0f - 1; p.vy = (rand() % 200) / 100.0f - 1; p.baseSize = 2 + (rand() % 20) / 10.0f; g_particles.push_back(p); } InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_GRAVITY: g_spectrumMode = SPEC_GRAVITY; g_gravInit = false; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_SYNTHWAVE: g_spectrumMode = SPEC_SYNTHWAVE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_SYMBIOTE: g_spectrumMode = SPEC_SYMBIOTE; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_VINYL: g_spectrumMode = SPEC_VINYL; g_vinylParticles.clear(); InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_SPIRAL: g_spectrumMode = SPEC_SPIRAL; g_spiralHistory.clear(); InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_WARP: g_spectrumMode = SPEC_WARP; g_warpTunnel.clear(); g_warpInit = false; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_PLANET: g_spectrumMode = SPEC_PLANET; g_planetInit = false; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_GRIND: g_spectrumMode = SPEC_GRIND; g_grindSparks.clear(); InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_BLIZZARD: g_spectrumMode = SPEC_BLIZZARD; g_blizzardInit = false; InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_SPECTRUM_WINDOW: g_spectrumMode = SPEC_WINDOW; g_blizzardInit = false; InvalidateRect(hwnd, nullptr, FALSE); break;

    case IDM_SET_THEME: g_isDarkMode = !g_isDarkMode; UpdateTitleBarTheme(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break;
    case IDM_SET_LYRICS: g_showLyrics = !g_showLyrics; if (g_showLyrics) { if (!g_currentFilePath.empty()) LoadAndParseLyrics(g_currentFilePath); else { g_lyricsData.clear(); g_lyricsDisplayStr = L"暂无正在播放的歌曲"; } } InvalidateRect(hwnd, nullptr, FALSE); break;
    }
    return 0;
  }
  case WM_KEYDOWN:
  {
    if (wParam == VK_SPACE) { TogglePlayback(); g_buttons[1].text = g_isPlaying ? L"\uE769" : L"\uE768"; InvalidateRect(hwnd, nullptr, FALSE); }
    else if (wParam == VK_LEFT) { if (!g_playlist.empty()) { int idx = g_currentIndex - 1; if (idx < 0) idx = (int)g_playlist.size() - 1; PlayTrackByIndex(idx, true); } }
    else if (wParam == VK_RIGHT) { if (!g_playlist.empty()) { int idx = g_currentIndex + 1; if (idx >= (int)g_playlist.size()) idx = 0; PlayTrackByIndex(idx, true); } }
    else if (wParam == VK_UP) { g_volume += 0.05f; if (g_volume > 1.0f) g_volume = 1.0f; if (g_stream) BASS_ChannelSetAttribute(g_stream, BASS_ATTRIB_VOL, g_volume); UpdateAppTitle(hwnd); }
    else if (wParam == VK_DOWN) { g_volume -= 0.05f; if (g_volume < 0.0f) g_volume = 0.0f; if (g_stream) BASS_ChannelSetAttribute(g_stream, BASS_ATTRIB_VOL, g_volume); UpdateAppTitle(hwnd); }
    else if (wParam == 'M') { SendMessage(hwnd, WM_COMMAND, IDM_SET_SIMPLEMODE, 0); }
    else if (wParam == 'T') { SendMessage(hwnd, WM_COMMAND, IDM_SET_THEME, 0); }
    else if (wParam == 'L') { SendMessage(hwnd, WM_COMMAND, IDM_SET_LYRICS, 0); }
    else if (wParam == 'V') {
      // 1. 计算下一个模式的索引
      int nextMode = g_spectrumMode + 1;
      // 枚举里最后一个是 SPEC_WINDOW (值是24)，如果超过它，就回到 SPEC_NONE (0)
      if (nextMode > SPEC_WINDOW) nextMode = SPEC_NONE;

      // 2. 计算对应的菜单 ID
      // IDM_SET_COVER 是 3000，IDM_SET_SPECTRUM_BAR 是 3001...
      // 刚好对应 3000 + mode
      int nextCommandID = IDM_SET_COVER + nextMode;

      // 3. 发送命令消息，这样可以自动触发 WM_COMMAND 里的粒子清理和初始化逻辑
      SendMessage(hwnd, WM_COMMAND, nextCommandID, 0);
    }
    return 0;
  }
  case WM_MOUSEWHEEL:
  {
    D2D1_POINT_2F ptDip = DpiHelper::ScreenToClientDips(hwnd, g_renderTarget);
    bool isScrollingText = !g_isSimpleMode && IsPointInRect(ptDip, g_textBoxRect) && !g_showLyrics;
    short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (isScrollingText) {
      float scrollStep = 30.0f;
      g_scrollOffset -= (float)zDelta / 120.0f * scrollStep;
      float boxHeight = g_textBoxRect.bottom - g_textBoxRect.top;
      float contentHeight = 0.0f;

      if (g_showLyrics) {
        // 歌词模式的高度
        contentHeight = g_textTotalHeight; // 实际上歌词是无限流动的，这里限制可能不太准，但够用
        // 对于流动歌词，允许用户滚动的范围
        contentHeight = (g_lyricsData.size() + 5) * 40.0f; // 估算
      }
      else if (!g_playlist.empty()) {
        // 播放列表模式的精确高度计算
        contentHeight = g_headerHeight + g_playlist.size() * LIST_ITEM_HEIGHT;
      }
      else {
        // 欢迎页
        contentHeight = g_textTotalHeight;
      }

      float maxOffset = std::max(0.0f, contentHeight - boxHeight + 20.0f);
      if (g_scrollOffset < 0.0f) g_scrollOffset = 0.0f;
      if (g_scrollOffset > maxOffset) g_scrollOffset = maxOffset;
    }
    else {
      g_volume += (float)zDelta / 120.0f * 0.01f;
      if (g_volume > 1.0f) g_volume = 1.0f; if (g_volume < 0.0f) g_volume = 0.0f;
      if (g_stream) BASS_ChannelSetAttribute(g_stream, BASS_ATTRIB_VOL, g_volume);
      UpdateAppTitle(hwnd);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  }
  case WM_LBUTTONDOWN:
  {
    POINT ptPixel = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    D2D1_POINT_2F ptDip = DpiHelper::PixelsToDips(ptPixel, g_renderTarget);
    if (!g_isSimpleMode && IsPointInRect(ptDip, g_progressBarRect) && g_stream) {
      float barWidth = g_progressBarRect.right - g_progressBarRect.left;
      if (barWidth > 0) {
        float ratio = (ptDip.x - g_progressBarRect.left) / barWidth; if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
        BASS_ChannelSetPosition(g_stream, (QWORD)(BASS_ChannelGetLength(g_stream, BASS_POS_BYTE) * ratio), BASS_POS_BYTE);
        g_musicTime = BASS_ChannelBytes2Seconds(g_stream, BASS_ChannelGetPosition(g_stream, BASS_POS_BYTE));
        g_smoothTime = g_musicTime;
        g_lastSysTime = std::chrono::steady_clock::now();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }
    bool anyHit = false; for (auto& btn : g_buttons) { if (IsPointInRect(ptDip, btn.rect)) { btn.isDown = true; anyHit = true; } }
    if (anyHit) { SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); }
    return 0;
  }
  case WM_LBUTTONUP:
  {
    POINT ptPixel = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    D2D1_POINT_2F ptDip = DpiHelper::PixelsToDips(ptPixel, g_renderTarget);

    // 播放列表点击逻辑
    if (!g_isSimpleMode && !g_showLyrics && !g_playlist.empty() && IsPointInRect(ptDip, g_textBoxRect)) {
      // 简单的数学计算：点击了哪一行
      float relativeY = ptDip.y - g_textBoxRect.top + g_scrollOffset;
      if (relativeY > g_headerHeight) {
        int clickedIndex = (int)((relativeY - g_headerHeight) / LIST_ITEM_HEIGHT);
        if (clickedIndex >= 0 && clickedIndex < (int)g_playlist.size()) {
          PlayTrackByIndex(clickedIndex, true); // 选中并播放
        }
      }
    }

    for (auto& btn : g_buttons) {
      if (btn.isDown) {
        btn.isDown = false; ReleaseCapture();
        if (IsPointInRect(ptDip, btn.rect)) {
          if (btn.id == 1 && !g_playlist.empty()) { int idx = g_currentIndex - 1; if (idx < 0) idx = (int)g_playlist.size() - 1; PlayTrackByIndex(idx); }
          else if (btn.id == 2) { TogglePlayback(); btn.text = g_isPlaying ? L"\uE769" : L"\uE768"; }
          else if (btn.id == 3 && !g_playlist.empty()) { int idx = g_currentIndex + 1; if (idx >= (int)g_playlist.size()) idx = 0; PlayTrackByIndex(idx); }
          else if (btn.id == 4) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_SET_PLAYLIST, L"选择歌单");
            HMENU hSubMenuMode = CreatePopupMenu();
            AppendMenuW(hSubMenuMode, MF_STRING, IDM_MODE_ORDER, L"顺序播放");
            AppendMenuW(hSubMenuMode, MF_STRING, IDM_MODE_LISTLOOP, L"列表循环");
            AppendMenuW(hSubMenuMode, MF_STRING, IDM_MODE_SINGLELOOP, L"单曲循环");
            int checkedID = IDM_MODE_ORDER;
            if (g_playMode == PM_LIST_LOOP) checkedID = IDM_MODE_LISTLOOP;
            else if (g_playMode == PM_SINGLE_LOOP) checkedID = IDM_MODE_SINGLELOOP;
            CheckMenuRadioItem(hSubMenuMode, IDM_MODE_ORDER, IDM_MODE_SINGLELOOP, checkedID, MF_BYCOMMAND);
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenuMode, L"播放模式");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            HMENU hSubMenuSpec = CreatePopupMenu();
            auto AddSpecItem = [&](UINT id, const wchar_t* name) { AppendMenuW(hSubMenuSpec, MF_STRING, id, name); };
            AddSpecItem(IDM_SET_COVER, L"显示封面");
            AppendMenuW(hSubMenuSpec, MF_SEPARATOR, 0, nullptr);
            AddSpecItem(IDM_SET_SPECTRUM_BAR, L"经典长条");
            AddSpecItem(IDM_SET_SPECTRUM_CIRC, L"宿命之环");
            AddSpecItem(IDM_SET_SPECTRUM_WAVE, L"动感波线");
            AddSpecItem(IDM_SET_SPECTRUM_BUBBLE, L"绚丽气泡");
            AddSpecItem(IDM_SET_SPECTRUM_GALAXY, L"星际粒子");
            AddSpecItem(IDM_SET_SPECTRUM_RADIAL, L"环形放射");
            AddSpecItem(IDM_SET_SPECTRUM_BLOCKS, L"赛博方块");
            AddSpecItem(IDM_SET_SPECTRUM_FIRE, L"比特篝火");
            AddSpecItem(IDM_SET_SPECTRUM_CITY, L"午夜霓虹");
            AddSpecItem(IDM_SET_SPECTRUM_RIPPLE, L"幻彩涟漪");
            AddSpecItem(IDM_SET_SPECTRUM_PIXEL, L"像素山丘");
            AddSpecItem(IDM_SET_SPECTRUM_LED, L"复古屏幕");
            AddSpecItem(IDM_SET_SPECTRUM_RAIN, L"像素暴雨");
            AddSpecItem(IDM_SET_SPECTRUM_NET, L"繁星之网");
            AddSpecItem(IDM_SET_SPECTRUM_GRAVITY, L"引力奇点");
            AddSpecItem(IDM_SET_SPECTRUM_SYNTHWAVE, L"赛博夕阳");
            AddSpecItem(IDM_SET_SPECTRUM_SYMBIOTE, L"液态生物");
            AddSpecItem(IDM_SET_SPECTRUM_VINYL, L"激光唱针");
            AddSpecItem(IDM_SET_SPECTRUM_SPIRAL, L"时空螺旋");
            AddSpecItem(IDM_SET_SPECTRUM_WARP, L"光速隧道");
            AddSpecItem(IDM_SET_SPECTRUM_PLANET, L"行星系统");
            AddSpecItem(IDM_SET_SPECTRUM_GRIND, L"虚空磨盘");
            AddSpecItem(IDM_SET_SPECTRUM_BLIZZARD, L"极地风暴");
            AddSpecItem(IDM_SET_SPECTRUM_WINDOW, L"静谧雪窗");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenuSpec, L"视觉效果");
            AppendMenuW(hMenu, MF_STRING, IDM_SET_LYRICS, g_showLyrics ? L"显示列表" : L"显示歌词");
            AppendMenuW(hMenu, MF_STRING, IDM_SET_SIMPLEMODE, g_isSimpleMode ? L"标准视图" : L"极简视图");
            AppendMenuW(hMenu, MF_STRING, IDM_SET_THEME, g_isDarkMode ? L"浅色主题" : L"深色主题");
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
          }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }
    return 0;
  }
  case WM_MOUSEMOVE:
  {
    POINT ptPixel = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    D2D1_POINT_2F ptDip = DpiHelper::PixelsToDips(ptPixel, g_renderTarget);

    // 播放列表悬停检测逻辑
    if (!g_isSimpleMode && !g_showLyrics && !g_playlist.empty() && IsPointInRect(ptDip, g_textBoxRect)) {
      float relativeY = ptDip.y - g_textBoxRect.top + g_scrollOffset;
      int oldHover = g_hoverIndex;
      g_hoverIndex = -1; // 默认无悬停

      // 只有当鼠标在 Header 下方时才计算
      if (relativeY > g_headerHeight) {
        int idx = (int)((relativeY - g_headerHeight) / LIST_ITEM_HEIGHT);
        if (idx >= 0 && idx < (int)g_playlist.size()) {
          g_hoverIndex = idx;
        }
      }

      // 如果悬停状态改变，才重绘 (优化性能)
      if (oldHover != g_hoverIndex) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }
    else {
      // 移出区域，取消高亮
      if (g_hoverIndex != -1) {
        g_hoverIndex = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
    }

    if (!g_isSimpleMode && IsPointInRect(ptDip, g_progressBarRect) && (wParam & MK_LBUTTON) && g_stream) {
      float ratio = (ptDip.x - g_progressBarRect.left) / (g_progressBarRect.right - g_progressBarRect.left);
      if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
      BASS_ChannelSetPosition(g_stream, (QWORD)(BASS_ChannelGetLength(g_stream, BASS_POS_BYTE) * ratio), BASS_POS_BYTE);
      g_musicTime = BASS_ChannelBytes2Seconds(g_stream, BASS_ChannelGetPosition(g_stream, BASS_POS_BYTE));
      g_smoothTime = g_musicTime;
      g_lastSysTime = std::chrono::steady_clock::now();
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    bool needRedraw = false; for (auto& btn : g_buttons) { bool h = IsPointInRect(ptDip, btn.rect); if (h != btn.isHover) { btn.isHover = h; needRedraw = true; } }
    if (needRedraw) InvalidateRect(hwnd, nullptr, FALSE);
    return 0;
  }
  case WM_PAINT: OnPaint(); return 0;
  case WM_COPYDATA:
  {
    PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
    if (pcds->dwData == 1) {
      std::wstring filePath = (wchar_t*)pcds->lpData;
      OpenAndPlayFile(filePath);
      if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
      SetForegroundWindow(hwnd);
    }
    return TRUE;
  }
  case WM_DESTROY:
    DiscardGraphicsResources();
    SafeRelease(&g_d2dFactory); SafeRelease(&g_dwriteFactory);
    SafeRelease(&g_textFormat); SafeRelease(&g_textFormatIcon); SafeRelease(&g_textFormatCenter); SafeRelease(&g_textFormatSmall); SafeRelease(&g_textFormatLyric);
    SafeRelease(&g_wicFactory); SafeRelease(&g_brushHighlight); SafeRelease(&g_brushDim); SafeRelease(&g_brushSpectrum);
    if (g_stream) BASS_StreamFree(g_stream); if (g_hFlacPlugin) BASS_PluginFree(g_hFlacPlugin); BASS_Free();
    PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// 业务逻辑
// ============================================================================
void LoadAndParseLyrics(const std::wstring& audioPath) {
  g_lyricsData.clear();
  g_lyricsDisplayStr = L"";
  if (audioPath.empty()) return;

  // 1. 尝试同目录下的 .lrc 文件
  std::wstring lrcPath = audioPath.substr(0, audioPath.find_last_of(L'.')) + L".lrc";
  HANDLE hFile = CreateFileW(lrcPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  // 2. 如果没找到，尝试 lyrics 子目录
  if (hFile == INVALID_HANDLE_VALUE) {
    size_t lastSlash = audioPath.find_last_of(L'\\');
    size_t lastDot = audioPath.find_last_of(L'.');
    if (lastSlash != std::wstring::npos) {
      std::wstring parentDir = audioPath.substr(0, lastSlash);
      std::wstring fileNameNoExt = (lastDot != std::wstring::npos && lastDot > lastSlash)
        ? audioPath.substr(lastSlash + 1, lastDot - lastSlash - 1)
        : audioPath.substr(lastSlash + 1);

      std::wstring subDirPath = parentDir + L"\\lyrics\\" + fileNameNoExt + L".lrc";
      hFile = CreateFileW(subDirPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
  }

  // 如果还是找不到，报错返回
  if (hFile == INVALID_HANDLE_VALUE) {
    g_lyricsDisplayStr = L"未找到歌词文件 (.lrc)";
    return;
  }

  // 3. 读取文件内容
  DWORD fileSize = GetFileSize(hFile, NULL);
  if (fileSize == 0) { g_lyricsDisplayStr = L"歌词文件为空"; CloseHandle(hFile); return; }

  std::vector<char> buffer(fileSize + 1);
  DWORD bytesRead;
  ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
  buffer[fileSize] = 0;
  CloseHandle(hFile);

  // 4. 转码 (UTF-8 或 ANSI)
  std::wstring wContent;
  int wLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, NULL, 0);
  if (wLen > 0) {
    std::vector<wchar_t> wBuffer(wLen);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, wBuffer.data(), wLen);
    wContent = wBuffer.data();
  }
  else {
    wLen = MultiByteToWideChar(CP_ACP, 0, buffer.data(), -1, NULL, 0);
    if (wLen > 0) {
      std::vector<wchar_t> wBuffer(wLen);
      MultiByteToWideChar(CP_ACP, 0, buffer.data(), -1, wBuffer.data(), wLen);
      wContent = wBuffer.data();
    }
  }

  if (wContent.empty()) return;

  // 5. 解析并合并双语
  std::wstringstream ss(wContent);
  std::wstring line;

  while (std::getline(ss, line)) {
    size_t posLeft = line.find(L'['), posRight = line.find(L']');
    if (posLeft != std::wstring::npos && posRight != std::wstring::npos && posRight > posLeft) {
      int min = 0; float sec = 0.0f;
      if (swscanf_s(line.substr(posLeft + 1, posRight - posLeft - 1).c_str(), L"%d:%f", &min, &sec) == 2) {
        double time = min * 60.0 + sec;
        std::wstring contentStr = line.substr(posRight + 1);
        // 去除回车换行
        while (!contentStr.empty() && (contentStr.back() == L'\r' || contentStr.back() == L'\n')) contentStr.pop_back();

        // === 核心：双语合并逻辑 ===
        bool merged = false;
        if (!g_lyricsData.empty()) {
          // 如果时间戳几乎相同 (误差小于 0.1秒) 且上一句没有译文，则认为是译文
          if (abs(time - g_lyricsData.back().time) < 0.1 && g_lyricsData.back().trans.empty()) {
            g_lyricsData.back().trans = contentStr;
            merged = true;
          }
        }

        if (!merged) {
          LyricEntry entry;
          entry.time = time;
          entry.text = contentStr;
          entry.trans = L""; // 初始化为空
          // 初始化默认值，后面 OnPaint 会重新计算
          entry.displayHeight = 40.0f;
          entry.origLines = 1;
          entry.transLines = 0;
          g_lyricsData.push_back(entry);

          // 顺便拼接到无歌词模式显示的字符串里
          g_lyricsDisplayStr += contentStr + L"\n";
        }
      }
    }
  }
  std::sort(g_lyricsData.begin(), g_lyricsData.end(), [](const LyricEntry& a, const LyricEntry& b) { return a.time < b.time; });
}

void ScanFolderForAudio(const std::wstring& folderPath) {
  g_playlist.clear(); g_currentIndex = -1;
  std::wstring searchPath = folderPath + L"\\*.*"; WIN32_FIND_DATAW findData; HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
  if (hFind == INVALID_HANDLE_VALUE) return;
  do {
    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      std::wstring fileName = findData.cFileName; std::wstring lowerName = fileName; std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), towlower);
      if ((lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == L".mp3") || (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == L".wav") ||
        (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == L".ogg") || (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == L".m4a") ||
        (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == L".aac") || (lowerName.length() >= 5 && lowerName.substr(lowerName.length() - 5) == L".flac"))
      {
        g_playlist.push_back(folderPath + L"\\" + fileName);
      }
    }
  } while (FindNextFileW(hFind, &findData)); FindClose(hFind);
  std::sort(g_playlist.begin(), g_playlist.end(), [](const std::wstring& a, const std::wstring& b) { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; });
  if (!g_playlist.empty()) { wchar_t iniPath[MAX_PATH]; GetModuleFileNameW(NULL, iniPath, MAX_PATH); PathRemoveFileSpecW(iniPath); PathAppendW(iniPath, L"config.ini"); WritePrivateProfileStringW(L"Settings", L"LastFolder", folderPath.c_str(), iniPath); }
}

std::wstring GetTruncatedString(const std::wstring& text, int limitWeight) {
  int currentWeight = 0; std::wstring result = L""; bool needEllipsis = false;
  for (size_t i = 0; i < text.length(); i++) {
    int charWeight = (text[i] < 128) ? 1 : 2;
    if (currentWeight + charWeight > limitWeight) { needEllipsis = true; break; }
    currentWeight += charWeight; result += text[i];
  }
  return needEllipsis ? result + L"..." : text;
}

void PlayTrackByIndex(int index, bool startPlaying) {
  if (g_playlist.empty() || index < 0 || index >= (int)g_playlist.size()) return;
  g_currentIndex = index;

  // 1. 释放旧音频，加载新音频 (这部分必须执行)
  if (g_stream) BASS_StreamFree(g_stream);
  g_currentFilePath = g_playlist[index];
  g_stream = BASS_StreamCreateFile(FALSE, g_currentFilePath.c_str(), 0, 0, BASS_UNICODE);

  if (g_stream) {
    BASS_ChannelSetAttribute(g_stream, BASS_ATTRIB_VOL, g_volume);

    // 重置时间变量
    g_musicTime = 0.0;
    g_smoothTime = 0.0;
    g_lastSysTime = std::chrono::steady_clock::now();

    if (startPlaying) {
      BASS_ChannelPlay(g_stream, FALSE);
      g_isPlaying = true;
      g_buttons[1].text = L"\uE769";
    }
    else {
      g_isPlaying = false;
      g_buttons[1].text = L"\uE768";
    }

    // --- 最小化时，跳过所有图形/界面操作 ---
    if (!g_isMinimized) {
      SafeRelease(&g_bitmap);
      if (!g_renderTarget) CreateGraphicsResources();

      // 只有显卡资源存在时，才去加载封面
      if (g_renderTarget) {
        HRESULT hr = LoadAlbumArtFromAudio(g_renderTarget, g_wicFactory, g_currentFilePath.c_str(), &g_bitmap);
        if (FAILED(hr) || !g_bitmap) LoadBitmapFromFile(g_renderTarget, g_wicFactory, L"cover.png", &g_bitmap);
      }

      // 界面文字更新
      std::wstring fileName = g_currentFilePath.substr(g_currentFilePath.find_last_of(L"\\") + 1);
      g_mainTextStr = L"正在播放 (" + std::to_wstring(index + 1) + L"/" + std::to_wstring(g_playlist.size()) + L"):\n" + fileName + L"\n\n播放列表";
      g_currentText = g_mainTextStr.c_str();

      // 滚动条定位
      if (!g_showLyrics) {
        float listStart = g_headerHeight;
        float targetY = listStart + index * LIST_ITEM_HEIGHT;
        float boxHeight = g_textBoxRect.bottom - g_textBoxRect.top;
        if (targetY < g_scrollOffset || targetY > g_scrollOffset + boxHeight - LIST_ITEM_HEIGHT) {
          g_scrollOffset = targetY - boxHeight / 2.0f;
          if (g_scrollOffset < 0) g_scrollOffset = 0;
        }
      }
    }
    // -----------------------------------------------------

    // 歌词解析不费资源，可以保留，或者也放到 if(!g_isMinimized) 里
    if (g_showLyrics) LoadAndParseLyrics(g_currentFilePath);

  }
  else {
    // 播放失败处理
    g_isPlaying = false;
    g_buttons[1].text = L"\uE768";
    // 最小化时不弹窗，否则会阻塞线程
    if (!g_isMinimized) MessageBoxW(g_hwnd, L"无法播放该文件", L"错误", MB_OK);
  }

  // 最小化时不重绘
  if (!g_isMinimized) InvalidateRect(g_hwnd, nullptr, FALSE);
}

void OpenAndPlayFile(const std::wstring& filePath) {
  if (filePath.empty()) return;
  wchar_t folderPath[MAX_PATH]; wcscpy_s(folderPath, filePath.c_str()); PathRemoveFileSpecW(folderPath);
  ScanFolderForAudio(folderPath);
  int targetIndex = 0;
  for (size_t i = 0; i < g_playlist.size(); i++) { if (g_playlist[i] == filePath) { targetIndex = (int)i; break; } }
  if (!g_playlist.empty()) PlayTrackByIndex(targetIndex, true);
}

void UpdateAppTitle(HWND hwnd) {
  int volVal = (int)(g_volume * 100 + 0.5f);
  std::wstring title = g_isSimpleMode ? L"音量: " + std::to_wstring(volVal) + L"%" : L"KeyPlayer - 音量: " + std::to_wstring(volVal) + L"%";
  SetWindowTextW(hwnd, title.c_str());
}

void UpdateTitleBarTheme(HWND hwnd) {
  // 1. 启用系统自带的“沉浸式深色模式” (Win10 1809+ / Win11)
  // 这会自动把标题栏变成深色，文字变成白色
  BOOL useDarkMode = g_isDarkMode ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

  // 2. 强制指定标题栏背景色 (仅 Win11 有效，Win10 会忽略此调用)
  // 如果是深色模式 -> 设为纯黑 (0x00000000)
  // 如果是浅色模式 -> 设为白色 (0x00FFFFFF)   系统默认色 (0xFFFFFFFF)
  COLORREF color = g_isDarkMode ? 0x00000000 : 0xFFFFFFFF;
  DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));

  // 3. 强制重绘非客户区 (标题栏)，确保颜色立即更新
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void TogglePlayback() {
  if (!g_stream) { if (!g_playlist.empty()) PlayTrackByIndex(0); else MessageBoxW(g_hwnd, L"请先加载歌单", L"提示", MB_OK); return; }
  if (BASS_ChannelIsActive(g_stream) == BASS_ACTIVE_PLAYING) { BASS_ChannelPause(g_stream); g_isPlaying = false; g_lastSysTime = std::chrono::steady_clock::now(); }
  else { BASS_ChannelPlay(g_stream, FALSE); g_isPlaying = true; }
}

// 辅助：计算一行文本在给定宽度下会折成几行
int CountLines(const std::wstring& text, float maxWidth, IDWriteTextFormat* pFormat, IDWriteFactory* pFactory) {
  if (text.empty()) return 0;
  IDWriteTextLayout* pLayout = nullptr;
  // 创建布局来测量
  HRESULT hr = pFactory->CreateTextLayout(text.c_str(), (UINT32)text.length(), pFormat, maxWidth, 5000.0f, &pLayout);
  int lines = 1;
  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_METRICS metrics;
    pLayout->GetMetrics(&metrics);
    // 如果很多行，按照每行约20px高度估算（或者更严谨地用 lineCount，但 DirectWrite 简单 metric 不直接给 lineCount，除非遍历）
    // 简单方法：高度 / 单行大致高度(约20-24)
    lines = (int)(metrics.height / 18.0f); // 18.0 是个经验值，略小于行高
    if (lines < 1) lines = 1;
    pLayout->Release();
  }
  return lines;
}

void OnPaint()
{
  if (g_isMinimized) return; // 最小化时不画
  HRESULT hr = CreateGraphicsResources(); if (FAILED(hr)) return;
  PAINTSTRUCT ps; BeginPaint(g_hwnd, &ps); g_renderTarget->BeginDraw();

  D2D1_COLOR_F colWinBg = g_isDarkMode ? D2D1::ColorF(D2D1::ColorF::Black) : D2D1::ColorF(D2D1::ColorF::White);
  D2D1_COLOR_F colBoxBg = g_isDarkMode ? D2D1::ColorF(D2D1::ColorF::Black) : D2D1::ColorF(D2D1::ColorF::White);
  D2D1_COLOR_F colText = g_isDarkMode ? D2D1::ColorF(D2D1::ColorF::Gray) : D2D1::ColorF(D2D1::ColorF::DimGray);

  g_renderTarget->Clear(colWinBg);

  // --- 浅色主题下的模糊背景 ---
  if (!g_isDarkMode && g_bitmap && g_renderTarget) {

    // 【关键点 1】分辨率降级：从 40 改为 12
    // 12x12 的分辨率足以提取主色调，但又低到足以抹平所有文字和细节
    // 如果你觉得颜色过渡还是有点硬，可以改为 8.0f；如果觉得颜色太单调，改为 16.0f
    D2D1_SIZE_F tinySize = D2D1::SizeF(12.0f, 12.0f);

    ID2D1BitmapRenderTarget* pTinyRT = nullptr;
    HRESULT hr = g_renderTarget->CreateCompatibleRenderTarget(tinySize, &pTinyRT);

    if (SUCCEEDED(hr) && pTinyRT) {
      pTinyRT->BeginDraw();
      pTinyRT->Clear(D2D1::ColorF(0, 0, 0, 0));

      // 抓取整个原图
      D2D1_SIZE_F bmpSize = g_bitmap->GetSize();
      D2D1_RECT_F sourceRect = D2D1::RectF(0, 0, bmpSize.width, bmpSize.height);
      D2D1_RECT_F destRect = D2D1::RectF(0, 0, tinySize.width, tinySize.height);

      // 压缩绘制
      pTinyRT->DrawBitmap(g_bitmap, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &sourceRect);

      pTinyRT->EndDraw();

      ID2D1Bitmap* pBlurBitmap = nullptr;
      hr = pTinyRT->GetBitmap(&pBlurBitmap);

      if (SUCCEEDED(hr) && pBlurBitmap) {
        D2D1_SIZE_F targetSize = g_renderTarget->GetSize();

        // 【关键点 2】强力放大裁剪 (Super Zoom)
        // 放大 1.5 倍 (150%)。
        // 作用：把边缘那些可能带有“线性拉伸痕迹”的部分推到屏幕外面去，
        // 只保留中间最柔和、色彩融合最好的部分。
        float zoom = 1.5f;

        float w = targetSize.width * zoom;
        float h = targetSize.height * zoom;
        float x = (targetSize.width - w) / 2.0f;
        float y = (targetSize.height - h) / 2.0f;

        D2D1_RECT_F bgRect = D2D1::RectF(x, y, x + w, y + h);

        // 拉伸铺满
        g_renderTarget->DrawBitmap(pBlurBitmap, bgRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        // 渐变蒙版 (保持不变，这是高级感的来源)
        ID2D1LinearGradientBrush* pGradientBrush = nullptr;
        D2D1_GRADIENT_STOP stops[2];

        // 顶部：0.4f (稍微让颜色透出来多一点，因为背景现在很柔和了)
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f);

        // 底部：0.95f (保持文字清晰)
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f);

        ID2D1GradientStopCollection* pStopCollection = nullptr;
        hr = g_renderTarget->CreateGradientStopCollection(stops, 2, &pStopCollection);

        if (SUCCEEDED(hr)) {
          hr = g_renderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
              D2D1::Point2F(0, 0),
              D2D1::Point2F(0, targetSize.height)
            ),
            pStopCollection,
            &pGradientBrush
          );

          if (SUCCEEDED(hr)) {
            D2D1_RECT_F fullScreen = D2D1::RectF(0, 0, targetSize.width, targetSize.height);
            g_renderTarget->FillRectangle(fullScreen, pGradientBrush);
            pGradientBrush->Release();
          }
          pStopCollection->Release();
        }
        pBlurBitmap->Release();
      }
      pTinyRT->Release();
    }
  }
  // -------------------------

  if (g_brush && g_textFormat) {
    // 1. 封面/频谱区域
    if (g_spectrumMode != SPEC_NONE) {
      g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
      g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);
      if (g_isPlaying && g_stream) {
        float fft[2048]; BASS_ChannelGetData(g_stream, fft, BASS_DATA_FFT2048);
        // ... (此处频谱绘制代码，完全保持原样即可) ...
        // --- 经典长条 (SPEC_BAR) ---
        if (g_spectrumMode == SPEC_BAR) {
          int numBars = SPEC_BARS;
          float barWidth = (g_imageBoxRect.right - g_imageBoxRect.left) / numBars;
          float heightScale = (g_imageBoxRect.bottom - g_imageBoxRect.top);
          for (int i = 0; i < numBars; i++) {
            int fftIdx = (int)(pow((float)i / numBars, 2.0f) * 200);
            if (fftIdx >= 1024) fftIdx = 1023;
            float val = fft[fftIdx] * 4.0f;
            if (val > 1.0f) val = 1.0f;
            if (val > g_fftPeaks[i]) { g_fftPeaks[i] = val; }
            else { g_fftPeaks[i] -= 0.03f; }
            if (g_fftPeaks[i] < 0) g_fftPeaks[i] = 0;
            float barHeight = g_fftPeaks[i] * heightScale;
            D2D1_RECT_F barRect = D2D1::RectF(g_imageBoxRect.left + i * barWidth + 1, g_imageBoxRect.bottom - barHeight, g_imageBoxRect.left + (i + 1) * barWidth - 1, g_imageBoxRect.bottom);
            if (g_brushSpectrum) {
              D2D1_POINT_2F start = D2D1::Point2F(0, g_imageBoxRect.bottom);
              D2D1_POINT_2F end = D2D1::Point2F(0, g_imageBoxRect.top);
              g_brushSpectrum->SetStartPoint(start); g_brushSpectrum->SetEndPoint(end);
              g_renderTarget->FillRectangle(barRect, g_brushSpectrum);
            }
            else {
              g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::DeepSkyBlue));
              g_renderTarget->FillRectangle(barRect, g_brush);
            }
          }
        }

        // --- 宿命之环 (SPEC_CIRCLE) ---
        else if (g_spectrumMode == SPEC_CIRCLE) {

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;
          float minDim = std::min(g_imageBoxRect.right - g_imageBoxRect.left, g_imageBoxRect.bottom - g_imageBoxRect.top);

          float radius = minDim * 0.35f;

          // 1. 深空背景 (绘制在框内)
          g_brush->SetColor(D2D1::ColorF(0x020205));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 2. 计算整体冲量 (Impulse)
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          float mid = (fft[10] + fft[11] + fft[12]) / 3.0f;

          // 冲量 = 低音(动力) + 中音(细节)
          float impulse = bass * 1.5f + mid * 0.5f;

          // --- 3. 核心：协调旋转速度 ---
          float rotSpeed = 1.5f + impulse * 15.0f;

          g_circleRotation += rotSpeed;
          if (g_circleRotation > 360.0f) g_circleRotation -= 360.0f;

          // --- 4. 发射逻辑 (切线向后) ---
          int samples = 60;
          for (int i = 0; i < samples; i++) {
            int fftIdx = (i * 2) % 100;
            float energy = fft[fftIdx] * 8.0f;

            if (energy > 0.2f) {
              float angleDeg = (float)i / samples * 360.0f + g_circleRotation;
              float rad = angleDeg * 3.14159f / 180.0f;

              float spawnX = cx + cos(rad) * radius;
              float spawnY = cy + sin(rad) * radius;

              // >>> 向后喷射向量 (逆时针) <<<
              float tanVx = sin(rad);
              float tanVy = -cos(rad);

              AccelParticle p;
              p.x = spawnX;
              p.y = spawnY;

              // 粒子速度与能量挂钩
              float pSpeed = 2.0f + energy * 8.0f;

              p.vx = tanVx * pSpeed;
              p.vy = tanVy * pSpeed;

              // 添加一点离心力 (向外飘)
              p.vx += cos(rad) * 2.0f;
              p.vy += sin(rad) * 2.0f;

              p.life = 1.0f;
              p.size = 1.5f + energy * 4.0f;

              // 颜色
              float hue = (float)i / samples + g_musicTime * 0.2f;
              float sat = (energy > 0.8f) ? 0.2f : 0.9f;
              p.color = HsvToRgb(hue, sat, 1.0f);

              g_accelParticles.push_back(p);
            }
          }

          // --- 5. 更新并绘制粒子 ---

          auto it = g_accelParticles.begin();
          while (it != g_accelParticles.end()) {
            it->x += it->vx;
            it->y += it->vy;
            it->vx *= 0.95f; // 阻力
            it->vy *= 0.95f;
            it->life -= 0.015f;

            // 增加边界检测 (上下左右各放宽 50px)
            bool outOfBounds = (it->x < g_imageBoxRect.left - 50.0f ||
              it->x > g_imageBoxRect.right + 50.0f ||
              it->y < g_imageBoxRect.top - 50.0f ||
              it->y > g_imageBoxRect.bottom + 50.0f);

            if (it->life <= 0.0f || outOfBounds) {
              it = g_accelParticles.erase(it);
            }
            else {
              g_brush->SetColor(it->color);
              g_brush->SetOpacity(it->life);

              float speed = sqrt(it->vx * it->vx + it->vy * it->vy);
              if (speed > 3.0f) {
                // 高速时画长拖尾
                D2D1_POINT_2F p1 = D2D1::Point2F(it->x, it->y);
                D2D1_POINT_2F p2 = D2D1::Point2F(it->x - it->vx * 0.5f, it->y - it->vy * 0.5f);
                g_renderTarget->DrawLine(p1, p2, g_brush, it->size);
              }
              else {
                g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(it->x, it->y), it->size / 2, it->size / 2), g_brush);
              }
              ++it;
            }
          }

          // --- 6. 绘制主环 ---

          float currentR = radius + impulse * 8.0f;

          // 1. 发光晕
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Cyan));
          g_brush->SetOpacity(0.2f + impulse * 0.5f);
          g_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), currentR, currentR), g_brush, 10.0f);

          // 2. 实体环
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(0.9f);
          g_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), currentR, currentR), g_brush, 2.0f);

          // 3. 机械卡扣 (随环旋转)
          D2D1::Matrix3x2F rotM = D2D1::Matrix3x2F::Rotation(g_circleRotation, D2D1::Point2F(cx, cy));
          g_renderTarget->SetTransform(rotM);

          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::DeepSkyBlue));
          g_brush->SetOpacity(0.8f);

          // 画三个连接点
          for (int i = 0; i < 3; i++) {
            float angle = i * 120.0f * 3.14159f / 180.0f;
            float px = cx + cos(angle) * currentR;
            float py = cy + sin(angle) * currentR;
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 6.0f, 6.0f), g_brush);
          }

          g_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

          g_brush->SetOpacity(1.0f);
        }

        // --- 动感波线 (SPEC_WAVE) ---
        else if (g_spectrumMode == SPEC_WAVE) {

          // 1. 背景：深邃的夜空黑
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;
          float width = g_imageBoxRect.right - g_imageBoxRect.left;

          // 2. 提取能量 (Bass/Mid/Treble) 用于驱动波浪幅度
          float energyBass = 0.0f;
          float energyMid = 0.0f;
          float energyHigh = 0.0f;

          for (int k = 1; k < 10; k++) energyBass += fft[k];
          for (int k = 10; k < 60; k++) energyMid += fft[k];
          for (int k = 60; k < 150; k++) energyHigh += fft[k];

          energyBass /= 9.0f;
          energyMid /= 50.0f;
          energyHigh /= 90.0f;

          energyBass = energyBass * 3.0f;
          energyMid = energyMid * 4.0f;
          energyHigh = energyHigh * 8.0f;

          struct WaveConfig {
            D2D1_COLOR_F color;
            float speed;
            float freq;
            float amplitude;
            float strokeWidth;
          };

          WaveConfig waves[] = {
              { D2D1::ColorF(D2D1::ColorF::Cyan), 2.0f, 3.0f, 0.4f + energyBass * 1.5f, 3.0f },
              { D2D1::ColorF(D2D1::ColorF::Magenta), 3.5f, 4.5f, 0.3f + energyMid * 1.2f, 2.0f },
              { D2D1::ColorF(D2D1::ColorF::Gold), 1.5f, 2.0f, 0.2f + energyHigh * 1.0f, 1.5f }
          };

          float step = 2.0f;

          for (int w = 0; w < 3; w++) {
            WaveConfig& cfg = waves[w];
            g_brush->SetColor(cfg.color);
            g_brush->SetOpacity(0.8f);

            D2D1_POINT_2F prevPt = D2D1::Point2F(g_imageBoxRect.left, cy);
            float timeOffset = g_musicTime * cfg.speed;

            for (float x = g_imageBoxRect.left; x <= g_imageBoxRect.right; x += step) {
              float normX = (x - g_imageBoxRect.left) / width * 2.0f - 1.0f;
              float envelope = 1.0f - (normX * normX);
              if (envelope < 0) envelope = 0;

              float waveVal = sin(normX * 3.14159f * cfg.freq + timeOffset);
              waveVal += sin(normX * 3.14159f * (cfg.freq * 2.0f) + timeOffset) * 0.5f;

              // 计算 Y 坐标
              float y = cy + waveVal * cfg.amplitude * envelope * (g_imageBoxRect.bottom - g_imageBoxRect.top) * 0.35f;

              D2D1_POINT_2F currentPt = D2D1::Point2F(x, y);

              if (x > g_imageBoxRect.left) {
                g_renderTarget->DrawLine(prevPt, currentPt, g_brush, cfg.strokeWidth);
              }
              prevPt = currentPt;
            }
          }
          g_brush->SetOpacity(1.0f);

          // 5. 加一条中间的辉光线
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(0.1f);
          g_renderTarget->DrawLine(
            D2D1::Point2F(g_imageBoxRect.left, cy),
            D2D1::Point2F(g_imageBoxRect.right, cy),
            g_brush, 1.0f
          );
          g_brush->SetOpacity(1.0f);

        }

        // --- 绚丽气泡 (SPEC_BUBBLE) ---
        else if (g_spectrumMode == SPEC_BUBBLE) {

          // 1. 分析低音能量 (Bass Energy)
          // 取 FFT 数组的前几项（低频部分）平均值
          float bassEnergy = 0.0f;
          for (int k = 1; k < 10; k++) bassEnergy += fft[k];
          bassEnergy /= 9.0f;
          // 放大系数，如果气泡太少，把这个 15.0f 改大；太多则改小
          bassEnergy *= 15.0f;

          // 2. 生成新气泡 (当低音超过阈值时)
          // 阈值 0.2f 防止静音时也乱冒泡
          if (bassEnergy > 0.2f) {
            // 限制一下生成频率，别一次生成太多
            if ((rand() % 10) < 4) {
              Bubble b;
              float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
              float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

              // 随机 X 位置
              b.x = g_imageBoxRect.left + (float)(rand() % 100) / 100.0f * boxW;
              // 初始 Y 位置在底部
              b.y = g_imageBoxRect.bottom;
              // 半径和能量挂钩
              b.radius = 5.0f + bassEnergy * 20.0f;
              // 速度也和能量挂钩
              b.speed = 1.0f + bassEnergy * 2.0f;

              // 随机颜色 (HSV 转 RGB)
              float hue = (float)(rand() % 100) / 100.0f;
              b.color = HsvToRgb(hue, 0.7f, 1.0f);
              b.alpha = 0.8f; // 初始透明度

              g_bubbles.push_back(b);
            }
          }

          // 3. 更新并绘制所有气泡
          // 使用迭代器遍历，方便删除跑出屏幕的气泡
          auto it = g_bubbles.begin();
          while (it != g_bubbles.end()) {
            // 更新位置
            it->y -= it->speed;
            it->alpha -= 0.005f; // 慢慢变淡

            // 如果气泡跑出顶部，或完全透明，就删除
            if (it->y < g_imageBoxRect.top - it->radius || it->alpha <= 0.0f) {
              it = g_bubbles.erase(it);
            }
            else {
              // 绘制
              g_brush->SetColor(it->color);
              g_brush->SetOpacity(it->alpha); // 设置透明度

              D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(it->x, it->y), it->radius, it->radius);
              g_renderTarget->FillEllipse(ellipse, g_brush);

              // 给气泡加个白色亮边，更有立体感
              g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White, it->alpha * 0.5f));
              g_renderTarget->DrawEllipse(ellipse, g_brush, 1.5f);

              ++it;
            }
          }
          // 记得把画笔透明度还原，否则影响其他界面绘制
          g_brush->SetOpacity(1.0f);
        }

        // --- 星际粒子 (SPEC_GALAXY) ---
        else if (g_spectrumMode == SPEC_GALAXY) {

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 1. 计算低音能量 (Bass Energy)
          // 取前 5 个 FFT 数据点的平均值，这代表最重的低音
          float bassEnergy = 0.0f;
          for (int k = 1; k < 6; k++) bassEnergy += fft[k];
          bassEnergy /= 5.0f;

          // 2. 绘制中心的“恒星” (随音乐跳动)
          float coreRadius = 20.0f + bassEnergy * 80.0f; // 基础大小 + 动态大小

          // 画一个半透明的光晕
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::MediumPurple, 0.5f));
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), coreRadius * 1.2f, coreRadius * 1.2f), g_brush);

          // 画实心核心 (亮白色)
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), coreRadius * 0.8f, coreRadius * 0.8f), g_brush);

          // 3. 生成新粒子 (喷射!)
          // 能量越强，喷得越多
          int spawnCount = (int)(bassEnergy * 20.0f);
          if (spawnCount > 0) {
            for (int i = 0; i < spawnCount; i++) {
              StarParticle p;
              p.x = cx;
              p.y = cy;
              p.size = 2.0f + (float)(rand() % 100) / 100.0f * 4.0f; // 随机大小 2~6
              p.alpha = 1.0f;

              // 随机颜色：紫色、蓝色、青色系
              // 如果不喜欢紫色，可以把 hue 改成随机数，变成彩色爆发。
              //float hue = (float)(rand() % 100) / 100.0f;
              float hue = 0.5f + (float)(rand() % 100) / 100.0f * 0.3f; // 0.5~0.8 (青->紫)
              p.color = HsvToRgb(hue, 0.6f, 1.0f);

              // 关键：随机角度喷射
              float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
              float speed = 2.0f + bassEnergy * 10.0f + (float)(rand() % 100) / 20.0f; // 基础速度 + 爆发力

              p.vx = cos(angle) * speed;
              p.vy = sin(angle) * speed;

              g_stars.push_back(p);
            }
          }

          // 4. 更新并绘制所有粒子
          auto it = g_stars.begin();
          while (it != g_stars.end()) {
            // 移动
            it->x += it->vx;
            it->y += it->vy;

            // 衰减
            it->alpha -= 0.015f; // 消失速度

            // 简单的物理效果：让粒子稍微带点阻力，或者越飞越快？
            // 这里让它匀速飞行，配合 alpha 衰减已经很好看了

            // 边界检测：如果完全透明或飞出屏幕太远
            bool outOfBounds = (it->x < g_imageBoxRect.left - 50 || it->x > g_imageBoxRect.right + 50 ||
              it->y < g_imageBoxRect.top - 50 || it->y > g_imageBoxRect.bottom + 50);

            if (it->alpha <= 0.0f || outOfBounds) {
              it = g_stars.erase(it);
            }
            else {
              // 绘制
              g_brush->SetColor(it->color);
              g_brush->SetOpacity(it->alpha);

              D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(it->x, it->y), it->size, it->size);
              g_renderTarget->FillEllipse(ellipse, g_brush);

              ++it;
            }
          }
          // 还原画笔透明度
          g_brush->SetOpacity(1.0f);
        }

        // --- 环形放射 (Radial Burst) ---
        else if (g_spectrumMode == SPEC_RADIAL) {
          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 基础半径：随低音震动
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          float radius = 40.0f + bass * 30.0f;

          int numBars = 60; // 柱子数量
          float angleStep = 360.0f / numBars;

          for (int i = 0; i < numBars; i++) {
            // 1. 获取高度
            int fftIdx = (int)(pow((float)i / numBars, 1.0f) * 100);
            float val = fft[fftIdx] * 6.0f;
            if (val > 1.0f) val = 1.0f;

            // 平滑处理
            if (val > g_fftPeaks[i]) g_fftPeaks[i] = val;
            else g_fftPeaks[i] -= 0.05f;
            if (g_fftPeaks[i] < 0) g_fftPeaks[i] = 0;

            // 2. 计算角度 (弧度制)
            // -90度是为了让索引0从正上方开始，或者正下方
            float angleDeg = i * angleStep - 90.0f;
            float angleRad = angleDeg * 3.14159f / 180.0f;

            // 3. 计算起点和终点
            // 起点在圆周上
            float startX = cx + cos(angleRad) * radius;
            float startY = cy + sin(angleRad) * radius;

            // 终点向外延伸
            float barLen = 5.0f + g_fftPeaks[i] * 60.0f; // 长度
            float endX = cx + cos(angleRad) * (radius + barLen);
            float endY = cy + sin(angleRad) * (radius + barLen);

            // 4. 绘制
            // 颜色随角度变化，形成彩虹圈
            float hue = (float)i / numBars;
            g_brush->SetColor(HsvToRgb(hue, 0.8f, 1.0f));

            // 线宽设大一点，像柱子一样 (StrokeWidth = 4.0f)
            g_renderTarget->DrawLine(
              D2D1::Point2F(startX, startY),
              D2D1::Point2F(endX, endY),
              g_brush,
              4.0f, // 粗细
              nullptr // 样式默认圆头，会很好看
            );
          }

          // 中间画个空心圆遮住起点的瑕疵，更精致
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black)); // 或者是背景色
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius - 2, radius - 2), g_brush);
        }

        // --- 赛博方块 (Cyber Blocks) ---
        else if (g_spectrumMode == SPEC_BLOCKS) {

          int numCols = 32; // 列数 (不用太多)
          int maxRows = 20; // 最大行数 (高度)

          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

          float colWidth = boxW / numCols;
          float blockHeight = boxH / maxRows;

          // 方块之间留一点缝隙
          float gap = 2.0f;

          for (int i = 0; i < numCols; i++) {
            // 获取这一列应有的高度 (0.0 ~ 1.0)
            int fftIdx = (int)((float)i / numCols * 80);
            float val = fft[fftIdx] * 5.0f;
            if (val > 1.0f) val = 1.0f;

            // 平滑
            if (val > g_fftPeaks[i]) g_fftPeaks[i] = val;
            else g_fftPeaks[i] -= 0.05f; // 下落快一点，更有打击感
            if (g_fftPeaks[i] < 0) g_fftPeaks[i] = 0;

            // 计算这一列该亮几个方块
            int activeBlocks = (int)(g_fftPeaks[i] * maxRows);

            float x = g_imageBoxRect.left + i * colWidth;

            // 从下往上画方块
            for (int j = 0; j < maxRows; j++) {
              // 如果当前方块在激活范围内，就画亮的，否则画暗的(或者不画)
              bool isActive = (j < activeBlocks);

              if (isActive) {
                // 颜色逻辑：底部绿色 -> 中间黄色 -> 顶部红色
                D2D1_COLOR_F color;
                float ratio = (float)j / maxRows;

                if (ratio < 0.5f)      color = D2D1::ColorF(D2D1::ColorF::LimeGreen);
                else if (ratio < 0.8f) color = D2D1::ColorF(D2D1::ColorF::Yellow);
                else                   color = D2D1::ColorF(D2D1::ColorF::Red);

                g_brush->SetColor(color);
                g_brush->SetOpacity(1.0f); // 亮方块完全不透明
              }
              else {
                // 未激活的方块，画成很暗的灰色，形成“网格背景”感
                g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::DarkGray));
                g_brush->SetOpacity(0.2f); // 很淡
              }

              // 计算方块坐标 (注意Y是从下往上算的)
              float y = g_imageBoxRect.bottom - (j + 1) * blockHeight;

              D2D1_RECT_F blockRect = D2D1::RectF(
                x + gap,
                y + gap,
                x + colWidth - gap,
                y + blockHeight - gap
              );

              // 用圆角方块会更现代一点
              g_renderTarget->FillRoundedRectangle(
                D2D1::RoundedRect(blockRect, 2.0f, 2.0f),
                g_brush
              );
            }
          }
          // 还原透明度
          g_brush->SetOpacity(1.0f);
        }

        // --- 比特篝火 (Pixel Fire) ---
        else if (g_spectrumMode == SPEC_FIRE) {

          // 定义像素块的大小 (越小越细腻，越大越复古)
          float pixelSize = 6.0f;

          int numCols = (int)((g_imageBoxRect.right - g_imageBoxRect.left) / pixelSize);
          int maxRows = (int)((g_imageBoxRect.bottom - g_imageBoxRect.top) / pixelSize);

          for (int x = 0; x < numCols; x++) {
            // 1. 获取高度
            int fftIdx = (int)((float)x / numCols * 80);
            float val = fft[fftIdx] * 4.0f;
            if (val > 1.0f) val = 1.0f;

            // 平滑下落
            if (val > g_fftPeaks[x]) g_fftPeaks[x] = val;
            else g_fftPeaks[x] -= 0.04f; // 火焰熄灭得稍微快一点
            if (g_fftPeaks[x] < 0) g_fftPeaks[x] = 0;

            // 计算这一列有多少个像素要亮
            int activeRows = (int)(g_fftPeaks[x] * maxRows);

            // 增加一点随机抖动，模拟火焰闪烁
            if (activeRows > 0) {
              activeRows += (rand() % 3) - 1;
            }

            // 2. 从下往上画像素
            for (int y = 0; y < activeRows; y++) {
              // 计算颜色热度 (Heat)
              // y=0 是底部(最热), y=max 是顶部(最冷)
              float heat = (float)y / activeRows;

              D2D1_COLOR_F color;
              if (heat < 0.2f)      color = D2D1::ColorF(D2D1::ColorF::White);        // 核心白热
              else if (heat < 0.4f) color = D2D1::ColorF(D2D1::ColorF::Yellow);       // 内焰黄
              else if (heat < 0.7f) color = D2D1::ColorF(D2D1::ColorF::Orange);       // 外焰橘
              else                  color = D2D1::ColorF(D2D1::ColorF::OrangeRed);    // 焰尖红

              g_brush->SetColor(color);

              // 越往上，透明度越低（像烟一样消散）
              float alpha = 1.0f - (heat * 0.5f);
              g_brush->SetOpacity(alpha);

              // 计算像素坐标
              float drawX = g_imageBoxRect.left + x * pixelSize;
              float drawY = g_imageBoxRect.bottom - (y + 1) * pixelSize;

              // 留 1px 缝隙，增强像素感
              g_renderTarget->FillRectangle(
                D2D1::RectF(drawX, drawY, drawX + pixelSize - 1, drawY + pixelSize - 1),
                g_brush
              );
            }
          }
          // 记得还原透明度！
          g_brush->SetOpacity(1.0f);
        }

        // --- 午夜霓虹 (Midnight City) ---
        else if (g_spectrumMode == SPEC_CITY) {

          float buildingWidth = 12.0f; // 楼宽
          int numBuildings = (int)((g_imageBoxRect.right - g_imageBoxRect.left) / buildingWidth);

          // 预设一些赛博霓虹配色 (青、紫、洋红)
          D2D1_COLOR_F neonColors[] = {
              D2D1::ColorF(D2D1::ColorF::Cyan),
              D2D1::ColorF(D2D1::ColorF::Magenta),
              D2D1::ColorF(D2D1::ColorF::DeepSkyBlue),
              D2D1::ColorF(D2D1::ColorF::Lime)
          };

          for (int i = 0; i < numBuildings; i++) {
            // 获取高度
            int fftIdx = (int)((float)i / numBuildings * 100);
            float val = fft[fftIdx] * 5.0f;
            if (val > 1.0f) val = 1.0f;

            // 城市的高度变化不要太剧烈，稍微平滑一点
            if (val > g_fftPeaks[i]) g_fftPeaks[i] = val;
            else g_fftPeaks[i] -= 0.02f;
            if (g_fftPeaks[i] < 0) g_fftPeaks[i] = 0;

            float buildingHeight = g_fftPeaks[i] * (g_imageBoxRect.bottom - g_imageBoxRect.top) * 0.8f;

            // 至少有一点高度，像地平线
            if (buildingHeight < 10.0f) buildingHeight = 10.0f;

            float x = g_imageBoxRect.left + i * buildingWidth;
            float bottom = g_imageBoxRect.bottom;
            float top = bottom - buildingHeight;

            // 1. 画大楼主体 (深色背景)
            D2D1_RECT_F buildingRect = D2D1::RectF(x + 1, top, x + buildingWidth - 1, bottom);
            g_brush->SetColor(D2D1::ColorF(0x101020)); // 深蓝黑色
            g_brush->SetOpacity(0.9f);
            g_renderTarget->FillRectangle(buildingRect, g_brush);

            // 2. 画边框 (霓虹光晕)
            // 选一种霓虹色，根据楼的位置循环
            D2D1_COLOR_F neonColor = neonColors[i % 4];
            g_brush->SetColor(neonColor);
            g_brush->SetOpacity(0.6f);
            g_renderTarget->DrawRectangle(buildingRect, g_brush, 1.0f);

            // 3. 画窗户 (Pixel Windows)
            // 窗户大小
            float winSize = 3.0f;
            float gap = 2.0f;

            // 遍历这栋楼的内部区域
            for (float wy = bottom - winSize - gap; wy > top + gap; wy -= (winSize + gap)) {
              for (float wx = x + gap + 1; wx < x + buildingWidth - gap - 1; wx += (winSize + gap)) {

                // 随机决定窗户是否亮起 (模拟有人开灯)
                // 利用 hash 算法，保证同一帧、同一位置的窗户状态是固定的，不会乱闪
                // 但加上 g_musicTime 可以让它们随时间偶尔变化
                int seed = (int)(wx * wy + i * 100 + g_musicTime * 0.5f);
                bool isLit = (seed % 10) > 6; // 30% 几率亮灯

                if (isLit) {
                  // 越高的楼层，窗户越亮 (受 FFT 强度影响)
                  float ratio = (bottom - wy) / buildingHeight;

                  // 窗户颜色：根据霓虹色偏白一点
                  g_brush->SetColor(neonColor);
                  // 强度够大的时候，窗户特别亮
                  g_brush->SetOpacity(val > ratio ? 1.0f : 0.3f);

                  g_renderTarget->FillRectangle(
                    D2D1::RectF(wx, wy, wx + winSize, wy + winSize),
                    g_brush
                  );
                }
              }
            }
          }
          // 还原
          g_brush->SetOpacity(1.0f);
        }

        // --- 幻彩涟漪 (Hypnotic Ripple) ---
        else if (g_spectrumMode == SPEC_RIPPLE) {

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 计算屏幕对角线长度，作为圆环的最大半径
          float maxRadius = sqrt(pow(g_imageBoxRect.right - g_imageBoxRect.left, 2) +
            pow(g_imageBoxRect.bottom - g_imageBoxRect.top, 2)) * 0.6f;

          // 1. 获取当前低音强度 (Bass)
          float bass = 0.0f;
          for (int k = 1; k < 5; k++) bass += fft[k];
          bass /= 4.0f;

          // 2. 生成新圆环 (Beat Detection)
          DWORD now = GetTickCount();
          // 条件：低音够强 (>0.15) 且 距离上次生成超过 150ms (防止太密)
          if (bass > 0.15f && (now - g_lastRippleTime > 150)) {
            RippleRing r;
            r.radius = 10.0f; // 初始大小
            r.thickness = 2.0f + bass * 10.0f; // 初始粗细
            r.alpha = 1.0f;
            r.speed = 3.0f + bass * 5.0f; // 爆发速度

            // 颜色：随时间流逝自动变色 (彩虹循环)
            // g_musicTime 是播放进度秒数
            float hue = (float)((int)(g_musicTime * 0.5f * 100) % 100) / 100.0f;
            r.hue = hue;

            g_ripples.push_back(r);
            g_lastRippleTime = now;
          }

          // 3. 更新并绘制
          auto it = g_ripples.begin();
          while (it != g_ripples.end()) {

            // 扩散
            it->radius += it->speed;

            // 稍微减速，模拟阻力 (可选)
            it->speed *= 0.98f;
            if (it->speed < 1.0f) it->speed = 1.0f;

            // 慢慢消失
            it->alpha -= 0.005f;

            // 边界检查
            if (it->alpha <= 0.0f || it->radius > maxRadius) {
              it = g_ripples.erase(it);
            }
            else {
              // --- 关键：律动感 ---
              // 所有圆环的粗细，都会叠加当前的 bass 强度
              // 这样即使是外圈的圆环，也会随着现在的鼓点一起“跳动”
              float currentThick = it->thickness + (bass * 20.0f);

              g_brush->SetColor(HsvToRgb(it->hue, 0.7f, 1.0f));
              g_brush->SetOpacity(it->alpha);

              D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                D2D1::Point2F(cx, cy),
                it->radius,
                it->radius
              );

              // 绘制空心圆环
              g_renderTarget->DrawEllipse(ellipse, g_brush, currentThick);

              // 为了更炫酷，再画一层细的亮白线在圆环中间
              g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
              g_brush->SetOpacity(it->alpha * 0.8f);
              g_renderTarget->DrawEllipse(ellipse, g_brush, 1.0f);

              ++it;
            }
          }

          // 4. 中心画一个实心的“能量球”，遮住圆环生成的起点
          float centerBallSize = 10.0f + bass * 40.0f; // 随低音变大
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(0.9f);
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), centerBallSize, centerBallSize), g_brush);

          // 还原
          g_brush->SetOpacity(1.0f);
        }

        // --- 像素山丘 (8-Bit Hill) ---
        else if (g_spectrumMode == SPEC_PIXEL) {

          // 1. 定义像素网格大小
          float pixelSize = 8.0f; // 每个像素块的大小 (8x8)

          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

          int cols = (int)(boxW / pixelSize);
          int rows = (int)(boxH / pixelSize);

          // 计算低音能量 (用于太阳跳动)
          float bass = 0.0f;
          for (int k = 1; k < 5; k++) bass += fft[k];
          bass /= 4.0f;

          // --- 背景：绘制像素星星 ---
          // 利用简单的伪随机，不使用全局变量，每一帧根据坐标计算是否画星星
          // 加入 g_musicTime 让星星闪烁
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          for (int sy = 0; sy < rows / 2; sy++) { // 只在天空(上半部分)画
            for (int sx = 0; sx < cols; sx++) {
              // 简单的伪随机哈希
              int seed = (sx * 57 + sy * 131);
              if ((seed % 100) > 97) { // 3% 的几率有星星
                // 闪烁效果
                float twinkle = sin(g_musicTime * 5.0f + seed);
                if (twinkle > 0) {
                  g_brush->SetOpacity(twinkle);
                  g_renderTarget->FillRectangle(
                    D2D1::RectF(
                      g_imageBoxRect.left + sx * pixelSize + 2,
                      g_imageBoxRect.top + sy * pixelSize + 2,
                      g_imageBoxRect.left + sx * pixelSize + 4,
                      g_imageBoxRect.top + sy * pixelSize + 4
                    ),
                    g_brush
                  );
                }
              }
            }
          }
          g_brush->SetOpacity(1.0f); // 还原

          // --- 主体：像素太阳 (Sun) ---
          // 位置：右上角
          float sunBaseSize = 40.0f;
          float sunSize = sunBaseSize + bass * 60.0f; // 随低音变大
          // 强制像素化太阳的大小 (取整)
          sunSize = floor(sunSize / pixelSize) * pixelSize;

          float sunX = g_imageBoxRect.right - 60.0f;
          float sunY = g_imageBoxRect.top + 60.0f;

          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Gold));
          g_renderTarget->FillRectangle(
            D2D1::RectF(sunX - sunSize / 2, sunY - sunSize / 2, sunX + sunSize / 2, sunY + sunSize / 2),
            g_brush
          );
          // 太阳核心
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::OrangeRed));
          g_renderTarget->FillRectangle(
            D2D1::RectF(sunX - sunSize / 4, sunY - sunSize / 4, sunX + sunSize / 4, sunY + sunSize / 4),
            g_brush
          );

          // --- 地形：频谱山丘 ---
          for (int i = 0; i < cols; i++) {
            // 获取高度
            int fftIdx = (int)((float)i / cols * 100);
            float val = fft[fftIdx] * 5.0f;
            if (val > 1.0f) val = 1.0f;

            // 平滑处理
            if (val > g_fftPeaks[i]) g_fftPeaks[i] = val;
            else g_fftPeaks[i] -= 0.05f;
            if (g_fftPeaks[i] < 0) g_fftPeaks[i] = 0;

            // 计算像素化的高度
            float rawHeight = g_fftPeaks[i] * boxH * 0.8f;
            // 关键：强制取整到 pixelSize 的倍数
            float quantizedHeight = floor(rawHeight / pixelSize) * pixelSize;

            if (quantizedHeight > 0) {
              float x = g_imageBoxRect.left + i * pixelSize;
              float bottom = g_imageBoxRect.bottom;
              float top = bottom - quantizedHeight;

              // 颜色：基于高度的渐变 (绿色 -> 黄色 -> 棕色) 模拟复古地图
              D2D1_COLOR_F blockColor;
              if (g_fftPeaks[i] < 0.3f)      blockColor = D2D1::ColorF(D2D1::ColorF::ForestGreen);
              else if (g_fftPeaks[i] < 0.6f) blockColor = D2D1::ColorF(D2D1::ColorF::LimeGreen);
              else                           blockColor = D2D1::ColorF(D2D1::ColorF::SaddleBrown); // 山顶

              g_brush->SetColor(blockColor);

              // 绘制一列中的每一个方块 (为了保留方块间的缝隙，看起来更像像素)
              for (float y = bottom - pixelSize; y >= top; y -= pixelSize) {
                g_renderTarget->FillRectangle(
                  D2D1::RectF(x + 1, y + 1, x + pixelSize - 1, y + pixelSize - 1),
                  g_brush
                );
              }

              // 山顶积雪效果 (如果是最高的那些)
              if (g_fftPeaks[i] > 0.7f) {
                g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                g_renderTarget->FillRectangle(
                  D2D1::RectF(x + 1, top + 1, x + pixelSize - 1, top + pixelSize - 1),
                  g_brush
                );
              }
            }
          }
        }

        // --- 复古 LED 屏 (Retro LED EQ) ---
        else if (g_spectrumMode == SPEC_LED) {

          float ledSize = 10.0f; // LED 灯珠稍微大一点
          float gap = 2.0f;      // 灯珠间距
          float step = ledSize + gap;

          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

          int cols = (int)(boxW / step);
          int rows = (int)(boxH / step);

          // 黑色背景
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          for (int x = 0; x < cols; x++) {
            // 获取 FFT 数据
            int fftIdx = (int)((float)x / cols * 100);
            float val = fft[fftIdx] * 4.0f;
            if (val > 1.0f) val = 1.0f;

            // 平滑
            if (val > g_fftPeaks[x]) g_fftPeaks[x] = val;
            else g_fftPeaks[x] -= 0.06f; // LED 熄灭比较快
            if (g_fftPeaks[x] < 0) g_fftPeaks[x] = 0;

            // 计算这一列亮几盏灯
            int activeLeds = (int)(g_fftPeaks[x] * rows);

            float drawX = g_imageBoxRect.left + x * step + (boxW - cols * step) / 2.0f; // 居中修正

            // 从下往上遍历每一行
            for (int y = 0; y < rows; y++) {

              float drawY = g_imageBoxRect.bottom - (y + 1) * step;
              D2D1_RECT_F ledRect = D2D1::RectF(drawX, drawY, drawX + ledSize, drawY + ledSize);

              // 判断这盏灯是亮还是灭
              bool isLit = (y < activeLeds);

              if (isLit) {
                // 亮灯颜色逻辑 (底部绿 -> 中间黄 -> 顶部红)
                D2D1_COLOR_F onColor;
                float ratio = (float)y / rows;

                if (ratio < 0.6f)      onColor = D2D1::ColorF(D2D1::ColorF::Lime);       // 绿
                else if (ratio < 0.85f) onColor = D2D1::ColorF(D2D1::ColorF::Yellow);     // 黄
                else                    onColor = D2D1::ColorF(D2D1::ColorF::Red);        // 红

                g_brush->SetColor(onColor);
                // 亮灯完全不透明，甚至可以加个高光(这里简化为纯色)
                g_renderTarget->FillRectangle(ledRect, g_brush);
              }
              else {
                // 灭灯状态：画一个很暗的灰色，形成“网格”背景感
                // 这样看起来就像屏幕上真的有灯珠，只是没亮
                g_brush->SetColor(D2D1::ColorF(0x222222));
                g_renderTarget->FillRectangle(ledRect, g_brush);
              }
            }
          }
        }

        // --- 像素暴雨 (Pixel Rain) ---
        else if (g_spectrumMode == SPEC_RAIN) {

          // 1. 设置黑色背景
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 2. 分析音频能量 (低音)
          float bass = 0.0f;
          for (int k = 1; k < 6; k++) bass += fft[k];
          bass /= 5.0f;

          // 3. 生成雨滴逻辑
          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float pixelGrid = 8.0f; // 像素网格宽度

          // A. 基础背景雨 (稀疏，慢速，暗色)
          if ((rand() % 100) < 15) {
            RainDrop r;
            int col = rand() % (int)(boxW / pixelGrid);
            r.x = g_imageBoxRect.left + col * pixelGrid;
            r.y = g_imageBoxRect.top - 20.0f; // 从框外一点生成
            r.speed = 4.0f + (float)(rand() % 5);
            r.length = 5.0f + (float)(rand() % 10);
            r.alpha = 0.3f;
            r.isHeavy = false;
            r.color = D2D1::ColorF(D2D1::ColorF::DarkGreen);
            g_rainDrops.push_back(r);
          }

          // B. 动态暴雨 (随低音生成，密集，快速，亮色)
          int spawnCount = (int)(bass * 30.0f);
          if (spawnCount > 0) {
            for (int i = 0; i < spawnCount; i++) {
              RainDrop r;
              int col = rand() % (int)(boxW / pixelGrid);
              r.x = g_imageBoxRect.left + col * pixelGrid;
              r.y = g_imageBoxRect.top - 50.0f; // 从更高处生成以获得加速感

              r.speed = 10.0f + bass * 20.0f + (float)(rand() % 5);
              r.length = 20.0f + bass * 50.0f;

              r.alpha = 0.8f + (bass * 0.2f);
              r.isHeavy = true;

              // 颜色动态变化：高能时刻变青/白
              if (bass > 0.4f) r.color = D2D1::ColorF(D2D1::ColorF::Cyan);
              else r.color = D2D1::ColorF(D2D1::ColorF::Lime);

              g_rainDrops.push_back(r);
            }
          }

          g_renderTarget->PushAxisAlignedClip(g_imageBoxRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

          // 4. 更新并绘制雨滴
          auto it = g_rainDrops.begin();
          while (it != g_rainDrops.end()) {
            // 更新位置
            it->y += it->speed;

            // 边界检测：完全掉出屏幕底部则删除
            if (it->y > g_imageBoxRect.bottom) {
              it = g_rainDrops.erase(it);
            }
            else {
              // 绘制主体
              D2D1_RECT_F rainRect = D2D1::RectF(
                it->x + 1,
                it->y,
                it->x + pixelGrid - 1,
                it->y + it->length
              );

              g_brush->SetColor(it->color);
              g_brush->SetOpacity(it->alpha);
              g_renderTarget->FillRectangle(rainRect, g_brush);

              // 绘制高亮头 (仅重雨滴)
              if (it->isHeavy) {
                g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                g_brush->SetOpacity(0.9f);
                g_renderTarget->FillRectangle(
                  D2D1::RectF(it->x + 1, it->y + it->length - pixelGrid, it->x + pixelGrid - 1, it->y + it->length),
                  g_brush
                );
              }

              // 绘制落地溅射 (当雨滴接近底部时)
              if (it->y + it->length >= g_imageBoxRect.bottom - 10.0f) {
                float splashSize = it->isHeavy ? 3.0f : 1.0f;
                g_brush->SetColor(it->color); // 用回雨滴颜色
                g_renderTarget->FillRectangle(
                  D2D1::RectF(it->x - splashSize, g_imageBoxRect.bottom - 3, it->x + pixelGrid + splashSize, g_imageBoxRect.bottom),
                  g_brush
                );
              }

              ++it;
            }
          }

          g_renderTarget->PopAxisAlignedClip();

          // 还原全局画笔透明度
          g_brush->SetOpacity(1.0f);
        }

        // --- 繁星之网 (Constellation / Network) ---
        else if (g_spectrumMode == SPEC_NET) {
          // 1. 深色背景 (略带一点深蓝，比纯黑更有质感)
          g_brush->SetColor(D2D1::ColorF(0x050510));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 2. 获取音乐能量 (低音 Bass)
          // 用于控制粒子的大小脉冲
          float bass = 0.0f;
          for (int k = 1; k < 5; k++) bass += fft[k];
          bass /= 4.0f;

          // 限制一下，防止太夸张
          if (bass > 0.8f) bass = 0.8f;

          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

          // 3. 更新粒子位置
          for (auto& p : g_particles) {
            p.x += p.vx;
            p.y += p.vy;

            // 碰到边界反弹
            if (p.x < g_imageBoxRect.left) { p.x = g_imageBoxRect.left; p.vx *= -1; }
            if (p.x > g_imageBoxRect.right) { p.x = g_imageBoxRect.right; p.vx *= -1; }
            if (p.y < g_imageBoxRect.top) { p.y = g_imageBoxRect.top; p.vy *= -1; }
            if (p.y > g_imageBoxRect.bottom) { p.y = g_imageBoxRect.bottom; p.vy *= -1; }
          }

          // 4. 绘制连线 (双重循环检测距离)
          // 连线颜色：青色/白色，带透明度
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Cyan));

          float connectDist = 70.0f; // 连接阈值距离

          for (size_t i = 0; i < g_particles.size(); i++) {
            for (size_t j = i + 1; j < g_particles.size(); j++) {

              float dx = g_particles[i].x - g_particles[j].x;
              float dy = g_particles[i].y - g_particles[j].y;
              float distSq = dx * dx + dy * dy;

              if (distSq < connectDist * connectDist) {
                float dist = sqrt(distSq);

                // 距离越近，线越不透明；距离越远，线越淡
                float alpha = 1.0f - (dist / connectDist);

                // 加上低音的闪烁感
                alpha *= (0.5f + bass * 2.0f);
                if (alpha > 1.0f) alpha = 1.0f;

                g_brush->SetOpacity(alpha);

                // 线条粗细也随低音变化
                float lineWidth = 1.0f + bass * 2.0f;

                g_renderTarget->DrawLine(
                  D2D1::Point2F(g_particles[i].x, g_particles[i].y),
                  D2D1::Point2F(g_particles[j].x, g_particles[j].y),
                  g_brush,
                  lineWidth
                );
              }
            }
          }

          // 5. 绘制粒子点
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(1.0f); // 粒子本身不透明

          for (auto& p : g_particles) {
            // 粒子大小 = 基础大小 + 音乐脉冲
            float r = p.baseSize + (bass * 8.0f);

            g_renderTarget->FillEllipse(
              D2D1::Ellipse(D2D1::Point2F(p.x, p.y), r, r),
              g_brush
            );
          }
        }

        // --- 引力奇点 (Gravitational) ---
        else if (g_spectrumMode == SPEC_GRAVITY) {

          // 1. 初始化粒子池 (切换到该模式时运行一次)
          if (!g_gravInit) {
            g_gravParticles.resize(GRAV_PARTICLE_COUNT);
            float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
            float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

            for (int i = 0; i < GRAV_PARTICLE_COUNT; i++) {
              float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
              float dist = 50.0f + (rand() % 100);
              g_gravParticles[i].x = cx + cos(angle) * dist;
              g_gravParticles[i].y = cy + sin(angle) * dist;
              g_gravParticles[i].vx = 0;
              g_gravParticles[i].vy = 0;
              g_gravParticles[i].hue = (float)(rand() % 100) / 100.0f;
              g_gravParticles[i].mass = 0.5f + (float)(rand() % 10) / 10.0f;
            }
            g_gravInit = true;
          }

          // 2. 绘制深色背景
          g_brush->SetColor(D2D1::ColorF(0x020205));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 3. 分析音乐能量
          float bass = 0.0f;
          for (int k = 1; k < 5; k++) bass += fft[k];
          bass /= 4.0f;

          float treble = 0.0f;
          for (int k = 60; k < 100; k++) treble += fft[k];
          treble /= 40.0f;

          // 4. 物理模拟与绘制
          float repulsionForce = 0.0f;
          if (bass > 0.15f) repulsionForce = bass * 15.0f; // 爆炸强度

          for (auto& p : g_gravParticles) {
            float dx = p.x - cx;
            float dy = p.y - cy;
            float distSq = dx * dx + dy * dy;
            float dist = sqrt(distSq);

            // 力场计算
            float gravity = 0.3f / (dist + 10.0f);
            float push = repulsionForce / (dist * 0.1f + 1.0f);
            float spin = 0.02f + treble * 0.1f;

            float nx = -dx / (dist + 0.1f);
            float ny = -dy / (dist + 0.1f);
            float tx = -ny;
            float ty = nx;

            p.vx += (nx * gravity + nx * -push + tx * spin) / p.mass;
            p.vy += (ny * gravity + ny * -push + ty * spin) / p.mass;

            p.vx *= 0.94f;
            p.vy *= 0.94f;
            p.x += p.vx;
            p.y += p.vy;

            // 边界重生
            bool isDead = false;
            if (dist < 5.0f) isDead = true;
            if (p.x < g_imageBoxRect.left - 50 || p.x > g_imageBoxRect.right + 50 ||
              p.y < g_imageBoxRect.top - 50 || p.y > g_imageBoxRect.bottom + 50) isDead = true;

            if (isDead) {
              float angle = (float)(rand() % 360);
              float spawnDist = 100.0f + (rand() % 50);
              p.x = cx + cos(angle) * spawnDist;
              p.y = cy + sin(angle) * spawnDist;
              p.vx = -sin(angle) * 1.0f;
              p.vy = cos(angle) * 1.0f;
            }

            // 绘制粒子
            float speed = sqrt(p.vx * p.vx + p.vy * p.vy);
            float colorMix = speed * 0.2f; if (colorMix > 1.0f) colorMix = 1.0f;

            D2D1_COLOR_F baseCol = HsvToRgb(p.hue + g_musicTime * 0.1f, 0.8f, 1.0f);
            D2D1_COLOR_F finalCol = D2D1::ColorF(
              baseCol.r + colorMix,
              baseCol.g + colorMix,
              baseCol.b + colorMix,
              0.6f + bass * 0.4f
            );

            g_brush->SetColor(finalCol);

            if (speed > 2.0f) {
              g_renderTarget->DrawLine(D2D1::Point2F(p.x, p.y), D2D1::Point2F(p.x - p.vx * 2.0f, p.y - p.vy * 2.0f), g_brush, 1.5f);
            }
            else {
              g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(p.x, p.y), 1.5f, 1.5f), g_brush);
            }
          }

          // 5. 绘制黑洞中心
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 8.0f, 8.0f), g_brush);

          float ringSize = 8.0f + bass * 15.0f;
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(0.5f);
          g_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), ringSize, ringSize), g_brush, 1.0f);
          g_brush->SetOpacity(1.0f);
        }

        // --- 赛博夕阳 (SPEC_SYNTHWAVE) ---
        else if (g_spectrumMode == SPEC_SYNTHWAVE) {

          // 1. 背景：深紫到黑的渐变天空
          if (g_brushSpectrum) {
            D2D1_POINT_2F start = D2D1::Point2F(0, g_imageBoxRect.top);
            D2D1_POINT_2F end = D2D1::Point2F(0, g_imageBoxRect.bottom);
            // 这里需要重新创建临时的渐变画笔来实现特定的天空色，或者简单用深紫色填充
            g_brush->SetColor(D2D1::ColorF(0x100020)); // 深紫色背景
            g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);
          }

          float width = g_imageBoxRect.right - g_imageBoxRect.left;
          float height = g_imageBoxRect.bottom - g_imageBoxRect.top;
          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float horizonY = g_imageBoxRect.top + height * 0.55f; // 地平线位置

          // 2. 绘制夕阳 (Sun)
          // 太阳随低音震动
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          float sunRadius = height * 0.25f + bass * 40.0f;

          // 太阳渐变色 (上黄下红)
          // 这里简单画一个实心圆，然后画横条遮挡形成百叶窗效果
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::OrangeRed));
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, horizonY - height * 0.1f), sunRadius, sunRadius), g_brush);

          // 太阳中心的黄色高光
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Gold));
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, horizonY - height * 0.1f), sunRadius * 0.6f, sunRadius * 0.6f), g_brush);

          // 绘制横向遮罩条 (Retro Stripe Effect)
          g_brush->SetColor(D2D1::ColorF(0x100020)); // 和背景同色
          float stripeH = 2.0f;
          for (float y = horizonY - sunRadius; y < horizonY; y += (stripeH * 2.5f)) {
            // 越往下条纹越粗
            float progress = (y - (horizonY - sunRadius)) / sunRadius;
            float thickness = 1.0f + progress * 4.0f;
            g_renderTarget->FillRectangle(D2D1::RectF(cx - sunRadius, y, cx + sunRadius, y + thickness), g_brush);
          }

          // 3. 绘制远处的频谱山脉 (Behind the grid)
          // 把 FFT 数据镜像绘制在地平线上
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Magenta)); // 霓虹粉山脉
          for (int i = 0; i < 64; i++) {
            float val = fft[i] * 5.0f;
            if (val > 1.0f) val = 1.0f;

            // 左边山脉
            float barW = width / 128.0f;
            float xLeft = cx - i * barW;
            float h = val * height * 0.3f;
            g_renderTarget->FillRectangle(D2D1::RectF(xLeft - barW + 1, horizonY - h, xLeft, horizonY), g_brush);

            // 右边山脉 (镜像)
            float xRight = cx + i * barW;
            g_renderTarget->FillRectangle(D2D1::RectF(xRight, horizonY - h, xRight + barW - 1, horizonY), g_brush);
          }

          // 4. 绘制透视网格地面 (Grid)
          // 强制裁剪，防止线画出框
          g_renderTarget->PushAxisAlignedClip(g_imageBoxRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Cyan));
          g_brush->SetOpacity(0.5f); // 半透明网格

          // 纵向线 (从灭点发射)
          int numVLines = 12;
          for (int i = -numVLines; i <= numVLines; i++) {
            // 简单的透视放射
            float bottomX = cx + i * (width / numVLines) * 2.0f; // 底部很宽
            g_renderTarget->DrawLine(D2D1::Point2F(cx, horizonY), D2D1::Point2F(bottomX, g_imageBoxRect.bottom), g_brush, 1.0f);
          }

          // 横向线 (随时间向 viewer 移动)
          // speed 随低音变化，bass 越强跑得越快
          float speed = (g_musicTime * 0.8f) + bass * 0.1f;
          // 取小数部分模拟无限循环
          float offset = speed - floor(speed);

          // 透视公式：y = 1 / z
          // 用简化的指数分布来模拟近大远小
          for (float i = 0.0f; i < 1.0f; i += 0.1f) {
            float z = i + offset;
            if (z > 1.0f) z -= 1.0f;

            // z=0 是地平线，z=1 是屏幕底部
            // 使用 pow 让靠近地平线的线更密集
            float drawY = horizonY + pow(z, 2.5f) * (height * 0.45f);

            g_renderTarget->DrawLine(
              D2D1::Point2F(g_imageBoxRect.left, drawY),
              D2D1::Point2F(g_imageBoxRect.right, drawY),
              g_brush,
              1.0f + z * 1.5f // 越近线越粗
            );
          }

          g_brush->SetOpacity(1.0f);
          g_renderTarget->PopAxisAlignedClip();
        }

        // --- 液态生物 (SPEC_SYMBIOTE) ---
        else if (g_spectrumMode == SPEC_SYMBIOTE) {
          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 1. 背景：深色背景，让发光生物更显眼
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 2. 分析音乐能量
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          // 限制最大形变，防止生物炸裂
          if (bass > 0.8f) bass = 0.8f;

          // 3. 计算生物形状 (16个控制点)
          const int points = 16;
          D2D1_POINT_2F vertices[points];

          // 基础半径随低音呼吸 (60 ~ 90)
          float baseRadius = 60.0f + bass * 30.0f;

          for (int i = 0; i < points; i++) {
            // 映射 FFT 能量到顶点
            int fftIdx = i * 2; // 取低频段
            float val = fft[fftIdx] * 80.0f;
            if (val > 50.0f) val = 50.0f; // 削峰

            // 半径 = 基础呼吸 + 局部频率突起
            float r = baseRadius + val;

            // 自身缓慢旋转
            float angle = (float)i / points * 2 * 3.14159f + g_musicTime * 0.5f;

            vertices[i].x = cx + cos(angle) * r;
            vertices[i].y = cy + sin(angle) * r;
          }

          // 4. 构建贝塞尔路径
          ID2D1PathGeometry* pGeo = nullptr;
          g_d2dFactory->CreatePathGeometry(&pGeo);
          ID2D1GeometrySink* pSink = nullptr;
          pGeo->Open(&pSink);

          pSink->SetFillMode(D2D1_FILL_MODE_WINDING);

          // 平滑连接算法：取每两个顶点的中点作为路径的起点/终点，顶点作为贝塞尔控制点
          // 这样曲线绝对圆滑，不会有尖角
          D2D1_POINT_2F pLast = vertices[points - 1];
          D2D1_POINT_2F pFirst = vertices[0];
          D2D1_POINT_2F startPt = D2D1::Point2F((pLast.x + pFirst.x) / 2, (pLast.y + pFirst.y) / 2);

          pSink->BeginFigure(startPt, D2D1_FIGURE_BEGIN_FILLED);

          for (int i = 0; i < points; i++) {
            int next = (i + 1) % points;
            D2D1_POINT_2F pCurrent = vertices[i];
            D2D1_POINT_2F pNext = vertices[next];
            D2D1_POINT_2F midPoint = D2D1::Point2F((pCurrent.x + pNext.x) / 2, (pCurrent.y + pNext.y) / 2);

            // 二次贝塞尔：控制点是原始顶点，终点是线段中点
            pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(pCurrent, midPoint));
          }

          pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
          pSink->Close();
          SafeRelease(&pSink);

          // 5. 创建径向渐变画笔 (实现变色+立体感)
          // 核心颜色逻辑：
          // Hue (色相): 随时间流逝(g_musicTime) + 低音瞬移(bass)
          float hue = g_musicTime * 0.2f + bass * 0.2f;

          // 内核颜色：高亮，低饱和度 (接近白)
          D2D1_COLOR_F colorInner = HsvToRgb(hue, 0.4f, 1.0f);
          // 外壳颜色：深色，高饱和度
          D2D1_COLOR_F colorOuter = HsvToRgb(hue + 0.1f, 1.0f, 0.7f);

          ID2D1GradientStopCollection* pGradientStops = nullptr;
          D2D1_GRADIENT_STOP gradientStops[2];
          gradientStops[0].color = colorInner;
          gradientStops[0].position = 0.0f; // 中心
          gradientStops[1].color = colorOuter;
          gradientStops[1].position = 1.0f; // 边缘

          g_renderTarget->CreateGradientStopCollection(
            gradientStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pGradientStops);

          ID2D1RadialGradientBrush* pRadBrush = nullptr;
          if (pGradientStops) {
            g_renderTarget->CreateRadialGradientBrush(
              D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0), baseRadius + 30.0f, baseRadius + 30.0f),
              pGradientStops,
              &pRadBrush
            );
          }

          // 6. 绘制主体
          if (pRadBrush) {
            g_renderTarget->FillGeometry(pGeo, pRadBrush);
            SafeRelease(&pRadBrush); // 释放临时画笔
          }
          SafeRelease(&pGradientStops); // 释放Stop集合

          // 7. 绘制“果冻”高光 (模拟湿润质感)
          // 在左上角画一个缩小的、半透明的白色版本
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(0.3f);

          D2D1::Matrix3x2F highlightTrans =
            D2D1::Matrix3x2F::Scale(0.7f, 0.7f, D2D1::Point2F(cx, cy)) *
            D2D1::Matrix3x2F::Translation(-15.0f, -15.0f); // 向左上偏移

          g_renderTarget->SetTransform(highlightTrans);
          g_renderTarget->FillGeometry(pGeo, g_brush);
          g_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

          // 8. 绘制外发光轮廓
          // 颜色比主体更亮一点
          g_brush->SetColor(HsvToRgb(hue, 0.6f, 1.0f));
          g_brush->SetOpacity(0.8f);
          g_renderTarget->DrawGeometry(pGeo, g_brush, 3.0f); // 描边粗一点

          // 还原全局状态
          g_brush->SetOpacity(1.0f);
          SafeRelease(&pGeo);
        }

        // --- 激光唱针 (SPEC_VINYL) ---
        else if (g_spectrumMode == SPEC_VINYL) {

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 计算唱片半径 (稍微留点边距)
          float minDim = std::min(g_imageBoxRect.right - g_imageBoxRect.left, g_imageBoxRect.bottom - g_imageBoxRect.top);
          float recordRadius = minDim * 0.45f;
          float labelRadius = recordRadius * 0.35f;

          // 1. 深色背景
          g_brush->SetColor(D2D1::ColorF(0x050505));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 2. 更新唱片旋转角度
          // 33转/分 约为每秒 200度，这里每帧转一点
          float rotSpeed = 1.0f;
          // 如果有低音，转稍微快一点点，增加动感
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          g_vinylRotation += rotSpeed + bass * 2.0f;
          if (g_vinylRotation > 360.0f) g_vinylRotation -= 360.0f;

          // --- 开始绘制唱片 ---

          // 3. 唱片本体 (黑色圆盘)
          g_brush->SetColor(D2D1::ColorF(0x101010));
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), recordRadius, recordRadius), g_brush);

          // 4. 唱片沟槽 (反光质感)
          // 画一些同心圆，带有微弱的光泽
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Gray));
          g_brush->SetOpacity(0.1f);
          for (float r = labelRadius + 5.0f; r < recordRadius; r += 3.0f) {
            g_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), g_brush, 1.0f);
          }
          g_brush->SetOpacity(1.0f);

          // 5. 唱片封面 (Label) - 随唱片旋转
          // 把封面图片限制在一个圆里，并且旋转它
          {
            // 创建圆形遮罩几何
            ID2D1PathGeometry* pMaskGeo = nullptr;
            g_d2dFactory->CreatePathGeometry(&pMaskGeo);
            ID2D1GeometrySink* pSink = nullptr;
            pMaskGeo->Open(&pSink);
            pSink->BeginFigure(D2D1::Point2F(cx + labelRadius, cy), D2D1_FIGURE_BEGIN_FILLED);
            pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx - labelRadius, cy), D2D1::SizeF(labelRadius, labelRadius), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
            pSink->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx + labelRadius, cy), D2D1::SizeF(labelRadius, labelRadius), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            SafeRelease(&pSink);

            ID2D1Layer* pLayer = nullptr;
            g_renderTarget->CreateLayer(NULL, &pLayer);

            // 应用遮罩
            g_renderTarget->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), pMaskGeo), pLayer);

            // 计算旋转矩阵
            D2D1::Matrix3x2F rotMatrix = D2D1::Matrix3x2F::Rotation(g_vinylRotation, D2D1::Point2F(cx, cy));
            g_renderTarget->SetTransform(rotMatrix);

            if (g_bitmap) {
              // 绘制封面图 (居中)
              D2D1_RECT_F labelRect = D2D1::RectF(cx - labelRadius, cy - labelRadius, cx + labelRadius, cy + labelRadius);
              g_renderTarget->DrawBitmap(g_bitmap, labelRect);
            }
            else {
              // 没有封面时画个彩色圆
              g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::OrangeRed));
              g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), labelRadius, labelRadius), g_brush);
            }

            // 还原坐标系和遮罩
            g_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
            g_renderTarget->PopLayer();
            SafeRelease(&pLayer);
            SafeRelease(&pMaskGeo);

            // 画个中心孔
            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.0f, 3.0f), g_brush);
          }

          // --- 粒子系统：激光刻录 ---

          // 6. 定义唱针接触点位置
          // 假设唱针在右上方 (-45度 / 315度)
          float stylusAngleDeg = -45.0f;
          float stylusAngleRad = stylusAngleDeg * 3.14159f / 180.0f;

          // 唱针不仅在一个点，它会根据频率高低，在唱片半径上移动
          // 低音在外圈，高音在内圈 (类似真实的物理刻录)
          // 在这一帧生成新的粒子

          int samples = 20; // 每一帧生成多少个点
          for (int i = 0; i < samples; i++) {
            // 映射到 FFT 频率
            int fftIdx = i * 2;
            float val = fft[fftIdx] * 20.0f;

            // 只有足够响的声音才产生火花
            if (val > 0.1f) {
              VinylParticle p;

              // 初始角度就是唱针的角度
              p.angle = stylusAngleRad;

              // 距离：低频在外，高频在内
              // 映射 i (0~20) 到 (labelRadius ~ recordRadius)
              float ratio = (float)i / samples;
              p.dist = recordRadius - ratio * (recordRadius - labelRadius);

              p.size = 1.0f + val * 3.0f;
              p.alpha = 1.0f;

              // 颜色：随距离变化 (外圈蓝/紫，内圈红/黄)
              p.color = HsvToRgb(0.6f - ratio * 0.5f, 0.8f, 1.0f);
              // 加上一点随机亮度
              if (val > 0.8f) p.color = D2D1::ColorF(D2D1::ColorF::White);

              g_vinylParticles.push_back(p);
            }
          }

          // 7. 更新并绘制粒子
          auto it = g_vinylParticles.begin();
          while (it != g_vinylParticles.end()) {
            // 核心逻辑：粒子的角度随唱片旋转而增加！
            // 注意：这里是 += 弧度。1度 ≈ 0.017弧度
            it->angle += (rotSpeed + bass * 2.0f) * 3.14159f / 180.0f;

            // 衰减
            it->alpha -= 0.008f; // 慢慢消失，形成长尾迹

            if (it->alpha <= 0.0f) {
              it = g_vinylParticles.erase(it);
            }
            else {
              // 极坐标转笛卡尔坐标
              float px = cx + cos(it->angle) * it->dist;
              float py = cy + sin(it->angle) * it->dist;

              g_brush->SetColor(it->color);
              g_brush->SetOpacity(it->alpha);

              // 画点 (或者画短弧线效果更好，但点比较简单)
              // 为了增加“激光感”，如果音量大，画十字星
              if (it->size > 3.0f) {
                g_renderTarget->DrawLine(D2D1::Point2F(px - it->size, py), D2D1::Point2F(px + it->size, py), g_brush, 1.0f);
                g_renderTarget->DrawLine(D2D1::Point2F(px, py - it->size), D2D1::Point2F(px, py + it->size), g_brush, 1.0f);
              }
              else {
                g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), it->size / 2, it->size / 2), g_brush);
              }
              ++it;
            }
          }
          g_brush->SetOpacity(1.0f);

          // 8. 绘制唱针臂 (简单的激光束指示)
          // 从右上角射向接触点
          float contactX = cx + cos(stylusAngleRad) * recordRadius;
          float contactY = cy + sin(stylusAngleRad) * recordRadius;

          // 画一束激光
          if (g_brushSpectrum) {
            g_brushSpectrum->SetStartPoint(D2D1::Point2F(g_imageBoxRect.right, g_imageBoxRect.top)); // 屏幕右上角
            g_brushSpectrum->SetEndPoint(D2D1::Point2F(cx, cy)); // 指向圆心方向
            g_brushSpectrum->SetOpacity(0.5f + bass * 0.5f); // 随低音闪烁

            // 只画到唱片边缘上方一点
            g_renderTarget->DrawLine(
              D2D1::Point2F(g_imageBoxRect.right, g_imageBoxRect.top),
              D2D1::Point2F(cx + cos(stylusAngleRad) * (labelRadius), cy + sin(stylusAngleRad) * (labelRadius)), // 假装这是一根激光臂
              g_brushSpectrum,
              2.0f
            );
          }
          g_brush->SetOpacity(1.0f);
        }

        // --- 时空螺旋 (SPEC_SPIRAL) ---
        else if (g_spectrumMode == SPEC_SPIRAL) {

          // 1. 基础参数计算
          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;

          // 计算方框的最小半径 (限制螺旋不跑出这个范围)
          float minDim = std::min(g_imageBoxRect.right - g_imageBoxRect.left, g_imageBoxRect.bottom - g_imageBoxRect.top);
          float maxRadius = minDim / 2.0f - 5.0f; // 留5px边距

          // 螺旋参数
          float startRadius = 2.0f;  // 中心起始半径
          float radiusStep = 0.3f;   // 半径步进 (控制螺旋疏密，越小越密)
          float angleStep = 0.15f;   // 角度步进 (控制线条弯曲度)

          // 2. 计算最大容纳点数 (Max Capacity)
          // 当半径从 startRadius 增加到 maxRadius 时，需要多少个点？
          size_t maxPoints = (size_t)((maxRadius - startRadius) / radiusStep);

          // 3. 深邃背景
          g_brush->SetColor(D2D1::ColorF(0x000005));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 4. 数据采样 (Recording)
          if (g_isPlaying && g_stream) {
            // 计算能量
            float energy = 0.0f;
            for (int k = 1; k < 20; k++) energy += fft[k];
            energy /= 19.0f;
            energy *= 6.0f;
            if (energy > 1.0f) energy = 1.0f;

            // 计算颜色 (随时间流逝变色)
            float hue = (float)((int)(g_musicTime * 0.1f * 100) % 100) / 100.0f;

            SpiralNode node;
            node.intensity = energy;
            node.hue = hue;

            // 加入历史记录 (添加到末尾)
            g_spiralHistory.push_back(node);

            // --- 关键逻辑：滑动窗口 ---
            // 如果点数超过了屏幕能容纳的极限，就把最旧的点(头部)删掉
            // 这样 vector 的大小永远维持在 maxPoints 以内
            if (g_spiralHistory.size() > maxPoints) {
              g_spiralHistory.erase(g_spiralHistory.begin());
            }
          }

          // 5. 绘制螺旋 (Drawing)
          // 需要倒序映射：最新的点(vector末尾)画在中心，最旧的点(vector开头)画在边缘
          // 这样看起来就像音乐从中心“流”向边缘

          if (g_spiralHistory.size() > 1) {
            D2D1_POINT_2F prevPt = D2D1::Point2F(cx, cy);
            bool isFirst = true;

            // 从 0 到 size 遍历
            // i = 0 代表中心(最新)，i = size 代表边缘(最旧)
            for (size_t i = 0; i < g_spiralHistory.size(); i++) {

              // 获取对应的数据：vector 的末尾是最新数据
              // 所以 index = size - 1 - i
              size_t dataIndex = g_spiralHistory.size() - 1 - i;
              const auto& node = g_spiralHistory[dataIndex];

              // 计算螺旋坐标
              // 半径随 i 增大 (离中心越来越远)
              float r = startRadius + i * radiusStep;

              // 角度随 i 增大
              float angle = i * angleStep - g_musicTime * 2.0f; // 加上 g_musicTime 让整个螺旋旋转起来！

              float x = cx + cos(angle) * r;
              float y = cy + sin(angle) * r;
              D2D1_POINT_2F currentPt = D2D1::Point2F(x, y);

              if (!isFirst) {
                // 颜色处理：边缘稍微变暗，制造深邃感
                float fade = 1.0f - (float)i / maxPoints; // 0.0 ~ 1.0

                // 强度越大越亮
                float saturation = 1.0f - node.intensity * 0.6f;
                D2D1_COLOR_F col = HsvToRgb(node.hue, saturation, 1.0f);
                col.a = fade; // 边缘透明

                g_brush->SetColor(col);

                // 线条粗细：中间细，边缘粗，或者随音乐跳动
                float strokeWidth = 1.0f + node.intensity * 4.0f;

                g_renderTarget->DrawLine(prevPt, currentPt, g_brush, strokeWidth);
              }

              prevPt = currentPt;
              isFirst = false;
            }

            // 6. 绘制中心发光点 (Center Glow)
            // 遮住螺旋起点的瑕疵
            const auto& newestNode = g_spiralHistory.back();
            float centerPulse = 3.0f + newestNode.intensity * 10.0f;

            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.0f, 3.0f), g_brush);

            g_brush->SetOpacity(0.5f);
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), centerPulse, centerPulse), g_brush);
            g_brush->SetOpacity(1.0f);
          }
        }

        // --- 光速隧道 (SPEC_WARP) ---
        else if (g_spectrumMode == SPEC_WARP) {

          // 1. 基础参数
          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;
          float maxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float maxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

          // 2. 深空背景 (带一点极光蓝)
          g_brush->SetColor(D2D1::ColorF(0x000208));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 强制裁剪，防止飞出框外
          g_renderTarget->PushAxisAlignedClip(g_imageBoxRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

          // 3. 计算速度 (由低音控制)
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          // 基础速度 + 低音加速
          float speed = 0.005f + bass * 0.04f;

          // 4. 管理切片 (Update Logic)

          // A. 移动现有切片
          for (auto& frame : g_warpTunnel) {
            // z 轴增加，表示向 viewer 靠近
            // 使用指数增长让它看起来有加速感 (近大远小)
            frame.z += speed * (1.0f + frame.z * 2.0f);
          }

          // B. 删除飞出屏幕的切片
          // 使用 erase-remove idiom 或者简单的循环删除
          if (!g_warpTunnel.empty() && g_warpTunnel.front().z > 1.2f) {
            g_warpTunnel.erase(g_warpTunnel.begin());
          }

          // C. 生成新切片 (Spawn)
          // 如果队列为空，或者最后一个切片已经离中心有一段距离了，就生成新的
          // 这样能保证隧道密度均匀
          if (g_warpTunnel.empty() || g_warpTunnel.back().z > 0.02f) {
            WarpFrame frame;
            frame.z = 0.0f; // 从最远端出生

            // 记录频谱快照 (取前32个频段)
            for (int i = 0; i < 32; i++) {
              frame.fft[i] = fft[i];
            }

            // 随机颜色或渐变颜色
            // 随时间变化的霓虹色
            float hue = g_musicTime * 0.2f;
            // 如果低音强，偏向红色/白色
            float sat = 1.0f - bass * 0.5f;
            frame.color = HsvToRgb(hue, sat, 1.0f);

            g_warpTunnel.push_back(frame);
          }

          // 5. 绘制切片 (Render Logic)
          // 必须从后往前画 (Painter's Algorithm)，这样近的会盖住远的
          // 也就是从 vector 的末尾(z=0) 到 开头(z=1) ? 
          // 不，vector back 是最新的(z=0)，front 是最旧的(z=大)。
          // 所以应该从 back 遍历到 front。

          // 用于画连接线的四个角坐标 (上一帧的坐标)
          D2D1_POINT_2F prevTL, prevTR, prevBL, prevBR;
          bool hasPrev = false;

          // 倒序遍历：从最新的(远的)画到最旧的(近的)
          for (int i = (int)g_warpTunnel.size() - 1; i >= 0; i--) {
            WarpFrame& frame = g_warpTunnel[i];

            // --- 透视投影计算 ---
            // 使用简单的透视公式：scale = z^3 (让远处更密集，近处扩张快)
            // 为了防止 z=0，加个基数
            float perspective = frame.z * frame.z * frame.z;

            // 计算当前切片在屏幕上的大小
            float curW = maxW * perspective;
            float curH = maxH * perspective;

            // 计算切片的矩形
            float left = cx - curW / 2.0f;
            float right = cx + curW / 2.0f;
            float top = cy - curH / 2.0f;
            float bottom = cy + curH / 2.0f;

            D2D1_RECT_F rect = D2D1::RectF(left, top, right, bottom);

            // 设置透明度 (远处的淡，近处的实)
            float alpha = frame.z;
            if (alpha > 1.0f) alpha = 1.0f;
            g_brush->SetColor(frame.color);
            g_brush->SetOpacity(alpha);

            // A. 绘制方框
            // 线条粗细随距离变粗
            float stroke = 1.0f + frame.z * 4.0f;
            g_renderTarget->DrawRectangle(rect, g_brush, stroke);

            // B. 绘制频谱“墙壁” (Spectral Walls)
            // 在方框的四条边上画出频谱柱
            // 上下边：画16个柱子
            for (int k = 0; k < 16; k++) {
              float val = frame.fft[k] * 5.0f; // 能量
              if (val > 0.05f) {
                // 柱子长度，随透视缩放
                float len = val * (maxH * 0.3f) * perspective;

                // 上边 (向上长)
                float posX = left + (float)k / 16.0f * curW;
                float stepW = (curW / 16.0f) * 0.8f;
                g_renderTarget->FillRectangle(D2D1::RectF(posX, top - len, posX + stepW, top), g_brush);

                // 下边 (向下长)
                g_renderTarget->FillRectangle(D2D1::RectF(posX, bottom, posX + stepW, bottom + len), g_brush);
              }
            }
            // 左右边：另外16个柱子
            for (int k = 0; k < 16; k++) {
              float val = frame.fft[k + 16] * 5.0f;
              if (val > 0.05f) {
                float len = val * (maxW * 0.3f) * perspective;
                float posY = top + (float)k / 16.0f * curH;
                float stepH = (curH / 16.0f) * 0.8f;

                // 左边 (向左长)
                g_renderTarget->FillRectangle(D2D1::RectF(left - len, posY, left, posY + stepH), g_brush);

                // 右边 (向右长)
                g_renderTarget->FillRectangle(D2D1::RectF(right, posY, right + len, posY + stepH), g_brush);
              }
            }

            // C. 绘制四角连线 (Speed Lines)
            // 连接当前框和上一个框的角落，形成“隧道感”
            D2D1_POINT_2F currTL = D2D1::Point2F(left, top);
            D2D1_POINT_2F currTR = D2D1::Point2F(right, top);
            D2D1_POINT_2F currBL = D2D1::Point2F(left, bottom);
            D2D1_POINT_2F currBR = D2D1::Point2F(right, bottom);

            if (hasPrev) {
              // 画细线
              g_brush->SetOpacity(alpha * 0.5f);
              g_renderTarget->DrawLine(currTL, prevTL, g_brush, 1.0f);
              g_renderTarget->DrawLine(currTR, prevTR, g_brush, 1.0f);
              g_renderTarget->DrawLine(currBL, prevBL, g_brush, 1.0f);
              g_renderTarget->DrawLine(currBR, prevBR, g_brush, 1.0f);
            }

            // 记录当前坐标给下一次循环用
            prevTL = currTL; prevTR = currTR; prevBL = currBL; prevBR = currBR;
            hasPrev = true;
          }

          // 6. 绘制中心灭点光爆
          // 当重低音时，中心闪白光
          if (bass > 0.3f) {
            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            float glareSize = bass * 100.0f;
            // 使用径向渐变或者简单的低透明度圆
            g_brush->SetOpacity(bass * 0.4f);
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), glareSize, glareSize), g_brush);
          }

          g_brush->SetOpacity(1.0f);
          g_renderTarget->PopAxisAlignedClip();
        }

        // --- 行星系统 (SPEC_PLANET) ---
        else if (g_spectrumMode == SPEC_PLANET) {

          // 1. 基础参数
          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;
          float cx = g_imageBoxRect.left + boxW / 2.0f;
          float cy = g_imageBoxRect.top + boxH / 2.0f;
          float maxRadius = std::min(boxW, boxH) / 2.0f;

          // 2. 强制裁剪
          g_renderTarget->PushAxisAlignedClip(g_imageBoxRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

          // 3. 初始化星云粒子 (复用 g_planets)
          // 需要很多粒子，所以如果数量少，就重新生成
          if (!g_planetInit || g_planets.size() < 300) {
            g_planets.clear();
            for (int i = 0; i < 300; i++) {
              Planet p;
              p.angle = (float)(rand() % 360);

              // 距离呈高斯分布，聚集在中间一圈
              float rRatio = (float)(rand() % 100) / 100.0f;
              // 让粒子更多分布在 0.3 ~ 0.9 的区域，形成环状
              p.distance = maxRadius * (0.3f + rRatio * 0.6f);

              // 速度差异化：内圈快，外圈慢 (开普勒定律模拟)
              p.speed = (1.0f - rRatio) * 3.0f + 0.5f;

              p.size = 1.0f + (rand() % 20) / 10.0f; // 1.0 ~ 3.0 大小

              // 颜色：冷暖色调混合 (冰蓝 -> 紫 -> 火红)
              // 根据距离决定颜色：内圈热(红)，外圈冷(蓝)
              float hue = 0.6f - (1.0f - rRatio) * 0.6f; // 0.6(蓝) -> 0.0(红)
              if (hue < 0) hue += 1.0f;

              p.color = HsvToRgb(hue, 0.8f, 1.0f);
              // 不需要尾迹，清空节省内存
              p.trail.clear();

              g_planets.push_back(p);
            }
            g_planetInit = true;
          }

          // 4. 分析能量
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;     // 低音：控制斥力
          float mid = (fft[10] + fft[20] + fft[30]) / 3.0f;  // 中音：控制颜色亮度
          float treble = (fft[60] + fft[70] + fft[80]) / 3.0f;// 高音：控制自转速度

          // 限制一下，防止炸飞
          if (bass > 0.8f) bass = 0.8f;

          // 5. 绘制背景
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 6. 物理模拟与绘制循环
          // 核心：所有粒子围绕中心旋转，且受低音斥力影响

          for (int i = 0; i < g_planets.size(); i++) {
            Planet& p = g_planets[i];

            // --- A. 角度更新 (公转) ---
            // 基础速度 + 高音加速
            float currentSpeed = p.speed + treble * 15.0f;
            p.angle += currentSpeed;
            if (p.angle > 360.0f) p.angle -= 360.0f;

            // --- B. 半径更新 (呼吸) ---
            // 原始半径记录在 p.distance 里 (可以认为这是静止时的平衡位置)
            // 动态半径 = 原始半径 + 低音斥力
            // 斥力：越靠近中心(内圈)受到的推力越大
            float push = bass * 80.0f * (maxRadius / p.distance);
            float currentR = p.distance + push;

            // 添加一点随机抖动 (Turbulence)
            currentR += sin(g_musicTime * 5.0f + i) * 2.0f;

            // --- C. 坐标计算 ---
            float rad = p.angle * 3.14159f / 180.0f;

            // 把它压扁一点点，做成 2.5D 的盘状
            float x = cx + cos(rad) * currentR;
            float y = cy + sin(rad) * currentR * 0.7f; // Y轴压缩 0.7

            // --- D. 绘制粒子 ---
            // 亮度：随中频闪烁
            float alpha = 0.4f + mid * 2.0f;
            if (alpha > 1.0f) alpha = 1.0f;

            // 远近透视：Y轴在下方(前面)的粒子大且亮，上方的暗且小
            float zScale = 1.0f + (sin(rad) * 0.3f); // 0.7 ~ 1.3
            float drawSize = p.size * zScale;

            // 如果低音很强，所有粒子变白 (高能反应)
            D2D1_COLOR_F drawCol = p.color;
            if (bass > 0.4f) {
              drawCol.r += bass; drawCol.g += bass; drawCol.b += bass;
            }

            g_brush->SetColor(drawCol);
            g_brush->SetOpacity(alpha);
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), drawSize, drawSize), g_brush);

            // 偶尔画几条连接线 (星链效果)
            // 只连接相邻的粒子，且距离较近时
            if (i > 0) {
              Planet& prev = g_planets[i - 1];
              // 简单的距离判断，不用开方，优化性能
              float dx = x - (cx + cos((prev.angle) * 3.14 / 180) * prev.distance); // 粗略估算
              // 这里为了性能和视觉，只连接物理存储上相邻的粒子
              // 实际上会形成一种杂乱的网状结构
              if (abs(p.angle - prev.angle) < 10.0f && abs(p.distance - prev.distance) < 20.0f) {
                g_renderTarget->DrawLine(D2D1::Point2F(x, y),
                  D2D1::Point2F(cx + cos(prev.angle * 3.14 / 180) * (prev.distance + push),
                    cy + sin(prev.angle * 3.14 / 180) * (prev.distance + push) * 0.7f),
                  g_brush, 0.5f);
              }
            }
          }

          // 7. 绘制中心黑洞/类星体
          // 它是能量的源头，也是吞噬者
          float holeSize = maxRadius * 0.15f;

          // 吸积盘光晕 (Accretion Disk Glow)
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::OrangeRed));
          g_brush->SetOpacity(0.3f + bass * 0.5f); // 随低音爆闪
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), holeSize * 1.5f, holeSize * 1.5f * 0.7f), g_brush);

          // 黑洞本体
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_brush->SetOpacity(1.0f);
          g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), holeSize, holeSize * 0.7f), g_brush);

          g_brush->SetOpacity(1.0f);
          g_renderTarget->PopAxisAlignedClip();
        }

        // --- 虚空磨盘 (SPEC_GRIND) ---
        else if (g_spectrumMode == SPEC_GRIND) {

          float cx = (g_imageBoxRect.left + g_imageBoxRect.right) / 2.0f;
          float cy = (g_imageBoxRect.top + g_imageBoxRect.bottom) / 2.0f;
          float minDim = std::min(g_imageBoxRect.right - g_imageBoxRect.left, g_imageBoxRect.bottom - g_imageBoxRect.top);

          // 磨盘的缝隙半径
          float gapRadius = minDim * 0.35f;

          // 1. 工业风背景
          //g_brush->SetColor(D2D1::ColorF(0x0a0a0a)); // 深灰黑
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 2. 能量分析
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          float mid = (fft[10] + fft[20]) / 2.0f;

          // 3. 更新旋转 (摩擦感)
          // 基础速度较慢，低音时剧烈加速
          float speed = 0.5f + bass * 8.0f;
          g_innerAngle += speed;        // 内环顺时针
          g_outerAngle -= speed * 0.8f; // 外环逆时针 (稍慢)

          // --- 4. 绘制火花 (Spark System) ---
          // 只有当能量足够强(发生摩擦)时才产生火花
          if (bass > 0.15f) {
            int sparkCount = (int)(bass * 20.0f); // 能量越大火花越多
            for (int i = 0; i < sparkCount; i++) {
              GrindSpark s;
              // 火花产生在缝隙处，随机角度
              float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
              s.x = cx + cos(angle) * gapRadius;
              s.y = cy + sin(angle) * gapRadius;

              // 速度：切线方向 + 随机散射
              // 模拟顺时针甩出的惯性
              float tanX = -sin(angle);
              float tanY = cos(angle);
              float speedVar = 5.0f + (rand() % 100) / 10.0f;

              s.vx = tanX * speedVar + (rand() % 20 - 10) / 5.0f;
              s.vy = tanY * speedVar + (rand() % 20 - 10) / 5.0f;

              s.life = 1.0f;

              // 颜色：金色/橙色/白色 (高温金属色)
              if ((rand() % 10) > 7) s.color = D2D1::ColorF(D2D1::ColorF::White); // 偶尔有亮白火花
              else s.color = D2D1::ColorF(1.0f, 0.5f + (rand() % 50) / 100.0f, 0.0f); // 金橙色

              g_grindSparks.push_back(s);
            }
          }

          // 绘制并更新火花 (无需裁剪，让火花飞出屏幕)
          auto it = g_grindSparks.begin();
          while (it != g_grindSparks.end()) {
            it->x += it->vx;
            it->y += it->vy;
            it->vy += 0.2f; // 重力下落！增加物理真实感
            it->life -= 0.03f; // 消失得比较快

            if (it->life <= 0.0f) {
              it = g_grindSparks.erase(it);
            }
            else {
              g_brush->SetColor(it->color);
              g_brush->SetOpacity(it->life);
              // 画细长线段
              g_renderTarget->DrawLine(
                D2D1::Point2F(it->x, it->y),
                D2D1::Point2F(it->x - it->vx * 0.5f, it->y - it->vy * 0.5f),
                g_brush, 1.0f
              );
              ++it;
            }
          }

          // --- 5. 绘制磨盘 (The Grindstones) ---

          // 挤压变形：低音越重，内环越向外，外环越向内
          float innerR = gapRadius - 10.0f + bass * 15.0f;
          float outerR = gapRadius + 10.0f - bass * 15.0f;

          // A. 绘制内环 (齿轮状)
          int teeth = 60;
          for (int i = 0; i < teeth; i++) {
            // 映射 FFT
            int fftIdx = i % 40;
            float val = fft[fftIdx] * 20.0f; // 齿的高度

            float angleDeg = (float)i / teeth * 360.0f + g_innerAngle;
            float rad = angleDeg * 3.14159f / 180.0f;

            // 内环齿向外指
            float rStart = innerR - 20.0f;
            float rEnd = innerR + val;

            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::DarkGray)); // 金属灰
            g_brush->SetOpacity(0.8f);

            // 如果摩擦剧烈(bass大)，齿尖变红
            if (bass > 0.3f && val > 10.0f) {
              g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::OrangeRed));
            }

            D2D1_POINT_2F p1 = D2D1::Point2F(cx + cos(rad) * rStart, cy + sin(rad) * rStart);
            D2D1_POINT_2F p2 = D2D1::Point2F(cx + cos(rad) * rEnd, cy + sin(rad) * rEnd);

            g_renderTarget->DrawLine(p1, p2, g_brush, 3.0f);
          }

          // B. 绘制外环 (反向齿轮)
          for (int i = 0; i < teeth; i++) {
            int fftIdx = (i + 20) % 40; // 错开频段
            float val = fft[fftIdx] * 20.0f;

            float angleDeg = (float)i / teeth * 360.0f + g_outerAngle;
            float rad = angleDeg * 3.14159f / 180.0f;

            // 外环齿向内指
            float rStart = outerR + 20.0f;
            float rEnd = outerR - val;

            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Gray));
            g_brush->SetOpacity(0.6f);

            D2D1_POINT_2F p1 = D2D1::Point2F(cx + cos(rad) * rStart, cy + sin(rad) * rStart);
            D2D1_POINT_2F p2 = D2D1::Point2F(cx + cos(rad) * rEnd, cy + sin(rad) * rEnd);

            g_renderTarget->DrawLine(p1, p2, g_brush, 3.0f);
          }

          // C. 绘制缝隙高亮 (Friction Ring)
          // 当两环接近时，缝隙发光
          float friction = bass; // 摩擦强度
          if (friction > 0.1f) {
            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Gold));
            g_brush->SetOpacity(friction); // 随摩擦强度变亮
            g_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), gapRadius, gapRadius), g_brush, 2.0f + friction * 5.0f);
          }

          // 恢复不透明度
          g_brush->SetOpacity(1.0f);
        }

        // --- 极地风暴 (SPEC_BLIZZARD) ---
        else if (g_spectrumMode == SPEC_BLIZZARD) {

          float boxW = g_imageBoxRect.right - g_imageBoxRect.left;
          float boxH = g_imageBoxRect.bottom - g_imageBoxRect.top;

          // 1. 初始化雪花
          if (!g_blizzardInit) {
            g_snowParticles.clear();
            g_snowParticles.resize(BLIZZARD_COUNT);
            for (int i = 0; i < BLIZZARD_COUNT; i++) {
              g_snowParticles[i].x = g_imageBoxRect.left + (rand() % (int)boxW);
              g_snowParticles[i].y = g_imageBoxRect.top + (rand() % (int)boxH);
              g_snowParticles[i].vx = 0;
              g_snowParticles[i].vy = 0;
              // 随机深度：小部分在近处(1.0)，大部分在远处(0.1)
              g_snowParticles[i].z = 0.1f + (float)(rand() % 90) / 100.0f;
              g_snowParticles[i].life = 1.0f;
            }
            g_blizzardInit = true;
          }

          // 2. 能量分析
          float bass = (fft[1] + fft[2] + fft[3]) / 3.0f;
          float treble = (fft[60] + fft[70]) / 2.0f;

          // 3. 绘制背景：寒冷的深蓝
          // 随低音闪烁雷电效果 (背景瞬间变亮)
          float lightning = 0.0f;
          if (bass > 0.4f) lightning = (bass - 0.4f) * 0.2f; // 闪电强度

          g_brush->SetColor(D2D1::ColorF(0.05f + lightning, 0.05f + lightning, 0.15f + lightning));
          g_renderTarget->FillRectangle(g_imageBoxRect, g_brush);

          // 4. 计算全球风力 (Wind Field)
          // 基础风：随时间缓慢左右摆动
          float baseWind = sin(g_musicTime * 0.5f) * 2.0f;

          // 暴风 gusts：由低音驱动的猛烈单向风
          // 如果低音强，风速暴增，方向随机突变
          static float gustDir = 1.0f;
          if (bass > 0.3f && (rand() % 20) == 0) gustDir *= -1.0f; // 偶尔变向

          float stormWind = bass * 30.0f * gustDir;

          // 5. 更新并绘制粒子
          // 强制裁剪
          g_renderTarget->PushAxisAlignedClip(g_imageBoxRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

          for (auto& p : g_snowParticles) {

            // --- 物理更新 ---

            // 重力：恒定下落，近处(z大)的落得快
            float gravity = 2.0f * p.z;

            // 风力受深度影响：近处的雪花受风影响更大
            float windForce = (baseWind + stormWind) * p.z;

            // 湍流 (Turbulence)：受高音影响的随机抖动
            float jitter = (treble * 5.0f) * ((rand() % 10 - 5) / 5.0f);

            // 应用速度 (带有惯性，不会瞬间改变，而是平滑过渡)
            // Lerp 插值：让当前速度慢慢接近目标速度
            float targetVx = windForce + jitter;
            float targetVy = gravity + bass * 5.0f; // 低音也会震落雪花

            p.vx += (targetVx - p.vx) * 0.1f;
            p.vy += (targetVy - p.vy) * 0.1f;

            // 更新位置
            p.x += p.vx;
            p.y += p.vy;

            // --- 边界循环 (Infinite Loop) ---
            // 如果飞出屏幕，从另一边绕回来，保持粒子总数不变
            if (p.x > g_imageBoxRect.right) p.x = g_imageBoxRect.left;
            if (p.x < g_imageBoxRect.left)  p.x = g_imageBoxRect.right;
            if (p.y > g_imageBoxRect.bottom) {
              p.y = g_imageBoxRect.top;
              // 重生时随机X
              p.x = g_imageBoxRect.left + (rand() % (int)boxW);
            }

            // --- 绘制 ---

            // 颜色：冰蓝色/白色
            // 远处(z小)暗淡，近处(z大)明亮
            float alpha = 0.3f + p.z * 0.7f;
            g_brush->SetColor(D2D1::ColorF(0.8f, 0.9f, 1.0f, alpha));

            // 计算总速度
            float speed = sqrt(p.vx * p.vx + p.vy * p.vy);

            // 视觉技巧：动态模糊 (Motion Blur)
            // 如果速度很快，画线条；如果速度慢，画圆点
            if (speed > 4.0f) {
              // 拉伸长度随速度变化
              float stretch = speed * 0.8f;
              // 逆着速度方向画线
              D2D1_POINT_2F p1 = D2D1::Point2F(p.x, p.y);
              // 归一化向量 * stretch
              D2D1_POINT_2F p2 = D2D1::Point2F(p.x - p.vx / speed * stretch, p.y - p.vy / speed * stretch);

              // 线条粗细随深度变化
              g_renderTarget->DrawLine(p1, p2, g_brush, 1.0f + p.z * 1.5f);
            }
            else {
              // 画雪球
              float size = 1.0f + p.z * 2.0f;
              g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(p.x, p.y), size, size), g_brush);
            }
          }

          // 6. 寒气迷雾 (Frost Fog)
          // 在屏幕底部画一层半透明的白色渐变，增加氛围感
          if (g_brushSpectrum) {
            D2D1_POINT_2F pStart = D2D1::Point2F(0, g_imageBoxRect.bottom);
            D2D1_POINT_2F pEnd = D2D1::Point2F(0, g_imageBoxRect.bottom - 50.0f);

            // 利用现有的线性画笔，但需要改一下颜色（这里为了简单，用纯色画笔模拟一层雾）
            g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            g_brush->SetOpacity(0.05f + bass * 0.1f); // 随低音变浓
            g_renderTarget->FillRectangle(
              D2D1::RectF(g_imageBoxRect.left, g_imageBoxRect.bottom - 60, g_imageBoxRect.right, g_imageBoxRect.bottom),
              g_brush
            );
          }

          g_renderTarget->PopAxisAlignedClip();
          g_brush->SetOpacity(1.0f);
        }

        // --- 静谧雪窗 (SPEC_WINDOW_SNOW) ---
        else if (g_spectrumMode == SPEC_WINDOW) {

          // 1. 定义绘制区域
          D2D1_RECT_F drawRect = g_imageBoxRect;
          float width = drawRect.right - drawRect.left;
          float height = drawRect.bottom - drawRect.top;

          // 2. 强制裁剪
          g_renderTarget->PushAxisAlignedClip(drawRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

          // 3. 绘制背景
          g_brush->SetColor(D2D1::ColorF(0.02f, 0.03f, 0.05f));
          g_renderTarget->FillRectangle(drawRect, g_brush);

          // 4. 音频能量
          float bass = 0.0f;
          for (int k = 1; k < 6; k++) bass += fft[k];
          bass /= 5.0f;
          static float smoothEnergy = 0.0f;
          smoothEnergy += (bass - smoothEnergy) * 0.08f;

          // 5. 初始化粒子
          if (!g_blizzardInit || g_snowParticles.size() != 1200) {
            g_snowParticles.clear();
            g_snowParticles.resize(1200);
            for (auto& p : g_snowParticles) {
              p.x = drawRect.left + (float)(rand() % (int)width);
              p.y = drawRect.top + (float)(rand() % (int)height);
              p.z = 0.1f + (float)(rand() % 90) / 100.0f;
              p.vx = (float)(rand() % 628) / 100.0f;
            }
            g_blizzardInit = true;
          }

          // 6. 绘制雪花
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          for (auto& p : g_snowParticles) {
            float speed = 0.2f + p.z * 1.3f;
            p.y += speed;
            p.x += sin(g_musicTime * 0.8f + p.vx) * (0.2f + p.z * 0.5f) * 0.1f;

            if (p.y > drawRect.bottom + 5.0f) {
              p.y = drawRect.top - 5.0f;
              p.x = drawRect.left + (float)(rand() % (int)width);
            }
            if (p.x > drawRect.right) p.x = drawRect.left;
            if (p.x < drawRect.left) p.x = drawRect.right;

            float baseSize = 1.0f + p.z * 2.0f;
            float bloomSize = baseSize + smoothEnergy * 8.0f * p.z;
            float alpha = (0.1f + p.z * 0.5f) + smoothEnergy * 0.4f;
            if (alpha > 0.95f) alpha = 0.95f;

            g_brush->SetOpacity(alpha * 0.4f);
            g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(p.x, p.y), bloomSize / 2.0f, bloomSize / 2.0f), g_brush);
            if (p.z > 0.4f) {
              float coreSize = baseSize * 0.8f;
              g_brush->SetOpacity(alpha * 0.9f);
              g_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(p.x, p.y), coreSize / 2.0f, coreSize / 2.0f), g_brush);
            }
          }

          // 7. 绘制扁平窗框 (保持不变)
          g_brush->SetOpacity(1.0f);
          D2D1_COLOR_F frameCol = g_isDarkMode ? D2D1::ColorF(0.08f, 0.07f, 0.06f) : D2D1::ColorF(0.3f, 0.25f, 0.2f);
          g_brush->SetColor(frameCol);

          float borderW = 12.0f;
          float gridW = 4.0f;

          // 四周
          g_renderTarget->FillRectangle(D2D1::RectF(drawRect.left, drawRect.top, drawRect.right, drawRect.top + borderW), g_brush);
          g_renderTarget->FillRectangle(D2D1::RectF(drawRect.left, drawRect.bottom - borderW, drawRect.right, drawRect.bottom), g_brush);
          g_renderTarget->FillRectangle(D2D1::RectF(drawRect.left, drawRect.top, drawRect.left + borderW, drawRect.bottom), g_brush);
          g_renderTarget->FillRectangle(D2D1::RectF(drawRect.right - borderW, drawRect.top, drawRect.right, drawRect.bottom), g_brush);

          // 十字
          float cx = (drawRect.left + drawRect.right) / 2.0f;
          float cy = (drawRect.top + drawRect.bottom) / 2.0f;
          g_renderTarget->FillRectangle(D2D1::RectF(cx - gridW / 2, drawRect.top, cx + gridW / 2, drawRect.bottom), g_brush);
          float barY = drawRect.top + (height * 0.4f);
          g_renderTarget->FillRectangle(D2D1::RectF(drawRect.left, barY - gridW / 2, drawRect.right, barY + gridW / 2), g_brush);

          // 8. 窗户玻璃反光 (还原为最初的简单反光)
          // 这一步完全去掉了复杂的锯齿胶带，改回右下角淡淡的白色反光条
          g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
          g_brush->SetOpacity(0.05f); // 极淡，不遮挡视线

          // 保存当前变换状态
          D2D1::Matrix3x2F oldTr; g_renderTarget->GetTransform(&oldTr);

          // 旋转45度，定位在右下角
          g_renderTarget->SetTransform(D2D1::Matrix3x2F::Rotation(45.0f, D2D1::Point2F(drawRect.right, drawRect.bottom)));

          // 画一个简单的长方形反光带
          g_renderTarget->FillRectangle(
            D2D1::RectF(drawRect.right - 150, drawRect.bottom, drawRect.right, drawRect.bottom + 40),
            g_brush
          );

          // 恢复变换状态
          g_renderTarget->SetTransform(oldTr);

          g_renderTarget->PopAxisAlignedClip();
          g_brush->SetOpacity(1.0f);
        }
        // ... (请确保所有其他频谱模式代码都在这里) ... 9527
        // 简单起见，假设保留了原代码的频谱逻辑，这里为了编译通过，添加了结束括号
        g_brush->SetOpacity(1.0f);
      }
      else {
        g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White)); g_renderTarget->DrawTextW(L"Spectrum", 8, g_textFormatCenter, g_imageBoxRect, g_brush);
      }
    }
    else {
      if (g_bitmap) { D2D1_RECT_F destRect = GetAspectFitRect(g_bitmap->GetSize(), g_imageBoxRect); g_renderTarget->DrawBitmap(g_bitmap, destRect); }
      else { g_brush->SetColor(colBoxBg); g_renderTarget->FillRectangle(g_imageBoxRect, g_brush); g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Gray)); g_renderTarget->DrawTextW(L"No Cover", 8, g_textFormatCenter, g_imageBoxRect, g_brush); }
    }

    // 2. 文本区域
    if (!g_isSimpleMode) {
      g_renderTarget->PushAxisAlignedClip(g_textBoxRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
      float boxHeight = g_textBoxRect.bottom - g_textBoxRect.top;
      float boxCenterY = g_textBoxRect.top + boxHeight / 2.0f;

      // 分支 A: 歌词模式
      if (g_showLyrics) {
        g_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

        // --- 平滑时间逻辑 ---
        auto currSysTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = currSysTime - g_lastSysTime;
        g_lastSysTime = currSysTime;
        double deltaTime = diff.count();
        if (abs(g_smoothTime - g_musicTime) > 0.15) g_smoothTime = g_musicTime;
        else { if (g_isPlaying) g_smoothTime += deltaTime; if (g_smoothTime > g_musicTime + 0.05) g_smoothTime = g_musicTime + 0.05; }
        double renderTime = g_smoothTime;
        // ------------------

        // 1. 确定当前歌词索引
        int currentIdx = -1;
        if (!g_lyricsData.empty()) {
          for (int i = 0; i < (int)g_lyricsData.size(); i++) {
            if (renderTime >= g_lyricsData[i].time) currentIdx = i;
            else break;
          }
        }

        // 2. 预计算可见区域歌词的高度 (使用内部 CalcLines 修复译文计算)
        float boxWidth = g_textBoxRect.right - g_textBoxRect.left;
        g_textFormatLyric->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

        int calcStart = std::max(0, currentIdx - 20);
        int calcEnd = std::min((int)g_lyricsData.size(), currentIdx + 20);

        // --- 定义内部高度测量函数 ---
        auto CalcLines = [&](const std::wstring& text) -> int {
          if (text.empty()) return 0;
          IDWriteTextLayout* pLayout = nullptr;
          HRESULT hr = g_dwriteFactory->CreateTextLayout(
            text.c_str(), (UINT32)text.length(), g_textFormatLyric, boxWidth, 1000.0f, &pLayout
          );
          int lines = 1;
          if (SUCCEEDED(hr)) {
            DWRITE_TEXT_METRICS metrics;
            pLayout->GetMetrics(&metrics);
            // 只要高度超过 22px，就认为是 2 行 (修复中文显示为1行的问题)
            if (metrics.height > 22.0f) lines = 2;
            pLayout->Release();
          }
          return lines;
          };

        for (int i = calcStart; i < calcEnd; i++) {
          // 计算原文和译文行数
          int linesOrig = CalcLines(g_lyricsData[i].text);
          if (linesOrig > 2) linesOrig = 2;

          int linesTrans = 0;
          if (!g_lyricsData[i].trans.empty()) {
            linesTrans = CalcLines(g_lyricsData[i].trans);
            if (linesTrans > 2) linesTrans = 2;
          }

          g_lyricsData[i].origLines = linesOrig;
          g_lyricsData[i].transLines = linesTrans;

          // 计算总高度
          float totalLines = (float)(linesOrig + linesTrans);
          float h = totalLines * 20.0f;
          if (h < 40.0f) h = 40.0f;
          g_lyricsData[i].displayHeight = h;
        }

        // 3. 计算滚动的像素偏移 (核心修改：改为线性匀速)
        float currentLineScrollY = 0.0f;
        if (currentIdx >= 0 && currentIdx < (int)g_lyricsData.size()) {
          double nextTime = (currentIdx + 1 < (int)g_lyricsData.size()) ? g_lyricsData[currentIdx + 1].time : (g_lyricsData[currentIdx].time + 5.0);
          double duration = nextTime - g_lyricsData[currentIdx].time;
          if (duration > 0.001) {
            double progress = (renderTime - g_lyricsData[currentIdx].time) / duration;
            if (progress > 1.0) progress = 1.0;

            // 这样就是匀速流动，不会有“快-慢-停”的感觉
            double flow = progress;

            // 当前行已经滚过的像素量
            currentLineScrollY = (float)flow * g_lyricsData[currentIdx].displayHeight;
          }
        }

        // 4. 渲染循环
        float boxCenterY = g_textBoxRect.top + (g_textBoxRect.bottom - g_textBoxRect.top) / 2.0f;

        if (currentIdx >= 0 && currentIdx < (int)g_lyricsData.size()) {
          // === 画当前行 ===
          float thisH = g_lyricsData[currentIdx].displayHeight;

          // 对齐逻辑改变
          // 效果：当前行的“播放点”永远在屏幕正中间
          float currentTopY = boxCenterY - currentLineScrollY;

          // 绘制函数
          auto DrawItem = [&](int idx, float topY) {
            float h = g_lyricsData[idx].displayHeight;
            if (topY > g_textBoxRect.bottom || topY + h < g_textBoxRect.top) return;

            bool isActive = (idx == currentIdx);
            ID2D1Brush* pBrush = isActive ? g_brushHighlight : g_brushDim;

            // 透明度计算
            float itemCenterY = topY + h / 2.0f;
            float distPixels = abs(itemCenterY - boxCenterY);
            // 稍微调大淡出范围，让更多歌词可见
            float opacity = exp(-0.00008f * distPixels * distPixels);
            if (isActive) opacity = 1.0f;
            else if (opacity < 0.2f) opacity = 0.2f;
            pBrush->SetOpacity(opacity);

            D2D1_RECT_F cellRect = D2D1::RectF(g_textBoxRect.left, topY, g_textBoxRect.right, topY + h);
            float lineHeightUnit = 20.0f;

            // 绘制原文
            float origH = g_lyricsData[idx].origLines * lineHeightUnit;
            float origY = topY;
            // 如果只有原文且高度不足40，垂直居中于 40px (美观修正)
            if (g_lyricsData[idx].trans.empty() && h <= 40.0f) {
              origY = topY + (h - origH) / 2.0f;
            }

            D2D1_RECT_F origRect = D2D1::RectF(cellRect.left, origY, cellRect.right, origY + origH);
            g_renderTarget->DrawTextW(
              g_lyricsData[idx].text.c_str(), (UINT32)g_lyricsData[idx].text.length(),
              g_textFormatLyric, origRect, pBrush,
              D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS_CLIP
            );

            // 绘制译文
            if (!g_lyricsData[idx].trans.empty()) {
              float transH = g_lyricsData[idx].transLines * lineHeightUnit;
              D2D1_RECT_F transRect = D2D1::RectF(cellRect.left, origY + origH, cellRect.right, origY + origH + transH);

              pBrush->SetOpacity(opacity * 0.8f);
              g_renderTarget->DrawTextW(
                g_lyricsData[idx].trans.c_str(), (UINT32)g_lyricsData[idx].trans.length(),
                g_textFormatLyric, transRect, pBrush,
                D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS_CLIP
              );
              pBrush->SetOpacity(opacity);
            }
            pBrush->SetOpacity(1.0f);
            };

          DrawItem(currentIdx, currentTopY);

          // 向下画
          float nextTopY = currentTopY + thisH;
          for (int i = currentIdx + 1; i < calcEnd; i++) {
            DrawItem(i, nextTopY);
            nextTopY += g_lyricsData[i].displayHeight;
            if (nextTopY > g_textBoxRect.bottom) break;
          }

          // 向上画
          float prevBottomY = currentTopY;
          for (int i = currentIdx - 1; i >= calcStart; i--) {
            float h = g_lyricsData[i].displayHeight;
            DrawItem(i, prevBottomY - h);
            prevBottomY -= h;
            if (prevBottomY < g_textBoxRect.top) break;
          }
        }

        g_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);
        if (g_lyricsData.empty()) {
          g_brushDim->SetOpacity(1.0f);
          g_renderTarget->DrawTextW(g_lyricsDisplayStr.c_str(), (UINT32)g_lyricsDisplayStr.length(), g_textFormatCenter, g_textBoxRect, g_brushDim);
        }
      }
      // 分支 B: 播放列表模式
      else if (!g_playlist.empty()) {
        // 1. 先绘制 Header (正在播放...)
        IDWriteTextLayout* pHeaderLayout = nullptr;
        float width = g_textBoxRect.right - g_textBoxRect.left;

        // 只要前面有内容，就绘制
        const wchar_t* headerText = g_mainTextStr.c_str(); // 此时 g_mainTextStr 只包含 header 信息

        hr = g_dwriteFactory->CreateTextLayout(headerText, (UINT32)wcslen(headerText), g_textFormat, width, 500.0f, &pHeaderLayout);
        if (SUCCEEDED(hr)) {
          DWRITE_TEXT_METRICS metrics; pHeaderLayout->GetMetrics(&metrics);
          //g_headerHeight = metrics.height + 5.0f; // 留点空隙
          g_headerHeight = metrics.height; // 不留空隙

          D2D1_POINT_2F origin = D2D1::Point2F(g_textBoxRect.left + 5.0f, g_textBoxRect.top + 5.0f - g_scrollOffset);

          // 只有当 header 在可见范围内才绘制
          if (origin.y + metrics.height > g_textBoxRect.top) {
            g_brush->SetColor(colText);
            g_renderTarget->DrawTextLayout(origin, pHeaderLayout, g_brush);
          }
          SafeRelease(&pHeaderLayout);
        }

        // 2. 逐行绘制播放列表
        float listStartY = g_textBoxRect.top + 5.0f - g_scrollOffset + g_headerHeight;
        int startIdx = 0;

        // 简单的视锥剔除 (Culling)，只绘制可见的
        // itemY = listStartY + i * LIST_ITEM_HEIGHT
        // 可见条件: itemY + height > boxTop && itemY < boxBottom
        // 推导 startIdx: listStartY + i*H > top  => i*H > top - listStartY => i > (top - listStartY)/H
        // 由于 listStartY 是基于 top 减去 scroll，简化为： i > (scroll - headerHeight)/H

        float effectiveScroll = g_scrollOffset - g_headerHeight;
        if (effectiveScroll > 0) startIdx = (int)(effectiveScroll / LIST_ITEM_HEIGHT);
        if (startIdx < 0) startIdx = 0;

        int endIdx = startIdx + (int)(boxHeight / LIST_ITEM_HEIGHT) + 2;
        if (endIdx > (int)g_playlist.size()) endIdx = (int)g_playlist.size();

        for (int i = startIdx; i < endIdx; i++) {
          float y = listStartY + i * LIST_ITEM_HEIGHT;

          // 生成显示文本 "01 SongName"
          std::wstring name = g_playlist[i].substr(g_playlist[i].find_last_of(L"\\") + 1);
          int num = i + 1;
          std::wstring prefix = (num < 10) ? (L"0" + std::to_wstring(num) + L" ") : (std::to_wstring(num) + L" ");
          std::wstring displayLine = prefix + name;

          // 截断过长文本
          //displayLine = GetTruncatedString(displayLine, 55); // 约55权重

          D2D1_RECT_F rowRect = D2D1::RectF(g_textBoxRect.left + 5.0f, y, g_textBoxRect.right, y + LIST_ITEM_HEIGHT);

          // 颜色逻辑
          if (i == g_currentIndex) {
            g_brushHighlight->SetOpacity(1.0f);
            g_renderTarget->DrawTextW(displayLine.c_str(), (UINT32)displayLine.length(), g_textFormat, rowRect, g_brushHighlight);
          }
          else if (i == g_hoverIndex) {
            // 悬停时也用蓝色，但稍微淡一点或者一样
            g_brushHighlight->SetOpacity(0.8f);
            g_renderTarget->DrawTextW(displayLine.c_str(), (UINT32)displayLine.length(), g_textFormat, rowRect, g_brushHighlight);
          }
          else {
            g_brush->SetColor(colText);
            g_renderTarget->DrawTextW(displayLine.c_str(), (UINT32)displayLine.length(), g_textFormat, rowRect, g_brush);
          }
        }
      }
      // 分支 C: 欢迎页 / 空白页
      else {
        float boxWidth = g_textBoxRect.right - g_textBoxRect.left;
        IDWriteTextLayout* pLayout = nullptr;
        const wchar_t* textPtr = (g_currentText && wcslen(g_currentText) > 0) ? g_currentText : g_mainTextStr.c_str();
        hr = g_dwriteFactory->CreateTextLayout(textPtr, (UINT32)wcslen(textPtr), g_textFormat, boxWidth - 10.0f, 200000.0f, &pLayout);
        if (SUCCEEDED(hr)) {
          DWRITE_TEXT_METRICS metrics; pLayout->GetMetrics(&metrics); g_textTotalHeight = metrics.height;
          D2D1_POINT_2F origin = D2D1::Point2F(g_textBoxRect.left + 5.0f, g_textBoxRect.top + 5.0f - g_scrollOffset);
          g_brush->SetColor(colText); g_renderTarget->DrawTextLayout(origin, pLayout, g_brush);
          SafeRelease(&pLayout);
        }
      }
      g_renderTarget->PopAxisAlignedClip();
    }

    // 3. 按钮
    for (const auto& btn : g_buttons) {
      D2D1_ROUNDED_RECT rBtn = D2D1::RoundedRect(btn.rect, 6.0f, 6.0f);

      // --- 背景颜色 ---
      // 逻辑：按下变深橙 -> 悬停变橙 -> 深色模式透明 -> 浅色模式蓝色
      D2D1_COLOR_F bgCol;
      if (btn.isDown)       bgCol = D2D1::ColorF(D2D1::ColorF::DarkOrange);
      else if (btn.isHover) bgCol = D2D1::ColorF(D2D1::ColorF::Orange);
      else if (g_isDarkMode) bgCol = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f); // 透明
      else                  bgCol = D2D1::ColorF(0x00AEEC);

      g_brush->SetColor(bgCol);
      g_brush->SetOpacity(1.0f);
      g_renderTarget->FillRoundedRectangle(rBtn, g_brush);

      // --- 边框颜色 ---
      // 逻辑：深色模式透明 -> 浅色模式 白色 White  或 D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.12f)
      D2D1_COLOR_F borderCol = g_isDarkMode ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.12f);

      g_brush->SetColor(borderCol);
      g_renderTarget->DrawRoundedRectangle(rBtn, g_brush, 0.8f);

      // --- 文字颜色 ---
      g_brush->SetColor(g_isDarkMode ? D2D1::ColorF(D2D1::ColorF::DarkGray) : D2D1::ColorF(D2D1::ColorF::White));
      g_renderTarget->DrawTextW(btn.text, (UINT32)wcslen(btn.text), g_textFormatIcon, btn.rect, g_brush);
    }

    // 4. 进度条
    if (!g_isSimpleMode && g_textFormatSmall) {
      D2D1_COLOR_F trackCol = g_isDarkMode ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.1f);
      g_brush->SetColor(trackCol);
      //g_brush->SetColor(g_isDarkMode ? D2D1::ColorF(D2D1::ColorF::DarkGray) : D2D1::ColorF(D2D1::ColorF::LightGray));
      g_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(g_progressBarRect, 6.0f, 6.0f), g_brush);
      double tot = 0, cur = 0; float r = 0;
      if (g_stream) { tot = BASS_ChannelBytes2Seconds(g_stream, BASS_ChannelGetLength(g_stream, BASS_POS_BYTE)); cur = g_musicTime; if (tot > 0.001) r = (float)(cur / tot); if (r > 1) r = 1; }
      if (r > 0) {
        D2D1_RECT_F clipRect = g_progressBarRect; clipRect.right = clipRect.left + (clipRect.right - clipRect.left) * r;
        g_renderTarget->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        // 0xB03060  Gray:(0x808080)
        g_brush->SetColor(g_isDarkMode ? D2D1::ColorF(0x808080) : D2D1::ColorF(0xFF6699));
        g_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(g_progressBarRect, 6.0f, 6.0f), g_brush);
        g_renderTarget->PopAxisAlignedClip();
      }
      std::wstring ts = FormatTime(cur) + L" / " + FormatTime(tot);
      g_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
      g_renderTarget->DrawTextW(ts.c_str(), (UINT32)ts.length(), g_textFormatSmall, g_progressBarRect, g_brush);
    }
  }
  g_renderTarget->EndDraw();
  EndPaint(g_hwnd, &ps);
}

void OnResize(UINT width, UINT height) { if (g_renderTarget) g_renderTarget->Resize(D2D1::SizeU(width, height)); }
HRESULT CreateGraphicsResources() {
  HRESULT hr = S_OK;
  if (!g_renderTarget) {
    RECT rc; GetClientRect(g_hwnd, &rc);
    hr = g_d2dFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(g_hwnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)), &g_renderTarget);
    if (SUCCEEDED(hr)) {
      hr = g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &g_brush);
      if (SUCCEEDED(hr)) hr = g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DeepSkyBlue), &g_brushHighlight);
      if (SUCCEEDED(hr)) hr = g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray), &g_brushDim);
      if (SUCCEEDED(hr)) {
        D2D1_GRADIENT_STOP stops[3]; stops[0].position = 0.0f; stops[0].color = D2D1::ColorF(D2D1::ColorF::DeepSkyBlue);
        stops[1].position = 0.5f; stops[1].color = D2D1::ColorF(D2D1::ColorF::Lime); stops[2].position = 1.0f; stops[2].color = D2D1::ColorF(D2D1::ColorF::Red);
        ID2D1GradientStopCollection* pStops = nullptr; hr = g_renderTarget->CreateGradientStopCollection(stops, 3, &pStops);
        if (SUCCEEDED(hr)) { hr = g_renderTarget->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 0)), pStops, &g_brushSpectrum); pStops->Release(); }
      }
    }
    if (SUCCEEDED(hr) && !g_bitmap) LoadBitmapFromFile(g_renderTarget, g_wicFactory, L"cover.png", &g_bitmap);
  }
  return hr;
}
void DiscardGraphicsResources() { SafeRelease(&g_brush); SafeRelease(&g_brushHighlight); SafeRelease(&g_brushDim); SafeRelease(&g_brushSpectrum); SafeRelease(&g_bitmap); SafeRelease(&g_renderTarget); }
HRESULT LoadBitmapFromFile(ID2D1RenderTarget* pRT, IWICImagingFactory* pWIC, PCWSTR uri, ID2D1Bitmap** ppBitmap) {
  IWICBitmapDecoder* decoder = nullptr; IWICBitmapFrameDecode* frame = nullptr; IWICFormatConverter* converter = nullptr;
  HRESULT hr = pWIC->CreateDecoderFromFilename(uri, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
  if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
  if (SUCCEEDED(hr)) hr = pWIC->CreateFormatConverter(&converter);
  if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
  if (SUCCEEDED(hr)) hr = pRT->CreateBitmapFromWicBitmap(converter, nullptr, ppBitmap);
  SafeRelease(&decoder); SafeRelease(&frame); SafeRelease(&converter); return hr;
}
std::wstring OpenFolderDialog(HWND hwnd) {
  std::wstring resultPath = L""; IFileOpenDialog* pFileOpen = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pFileOpen))) {
    DWORD dwOptions; if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
    if (SUCCEEDED(pFileOpen->Show(hwnd))) {
      IShellItem* pItem; if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
        PWSTR pszFilePath; if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) { resultPath = pszFilePath; CoTaskMemFree(pszFilePath); }
        pItem->Release();
      }
    }
    pFileOpen->Release();
  }
  return resultPath;
}
HRESULT LoadAlbumArtFromAudio(ID2D1RenderTarget* pRT, IWICImagingFactory* pWIC, PCWSTR filePath, ID2D1Bitmap** ppBitmap) {
  IShellItem* pItem = nullptr; HRESULT hr = SHCreateItemFromParsingName(filePath, nullptr, IID_PPV_ARGS(&pItem)); if (FAILED(hr)) return hr;
  IShellItemImageFactory* pImageFactory = nullptr; hr = pItem->QueryInterface(IID_PPV_ARGS(&pImageFactory)); HBITMAP hBitmap = nullptr;
  if (SUCCEEDED(hr)) { SIZE size = { 512, 512 }; hr = pImageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hBitmap); pImageFactory->Release(); }
  pItem->Release(); if (FAILED(hr) || !hBitmap) return E_FAIL;
  IWICBitmap* pWicBitmap = nullptr; hr = pWIC->CreateBitmapFromHBITMAP(hBitmap, nullptr, WICBitmapUseAlpha, &pWicBitmap); DeleteObject(hBitmap);
  if (SUCCEEDED(hr)) {
    IWICFormatConverter* pConverter = nullptr; hr = pWIC->CreateFormatConverter(&pConverter);
    if (SUCCEEDED(hr)) {
      hr = pConverter->Initialize(pWicBitmap, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
      if (SUCCEEDED(hr)) hr = pRT->CreateBitmapFromWicBitmap(pConverter, nullptr, ppBitmap);
      pConverter->Release();
    } pWicBitmap->Release();
  } return hr;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
  const wchar_t* MUTEX_NAME = L"MyMusicPlayer_Unique_Mutex_A46_ProgressBar";
  HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND hExistingWnd = FindWindowW(L"D2D_A42_Wave", nullptr);
    if (hExistingWnd) {
      int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
      if (argv && argc > 1) {
        std::wstring filePath = argv[1];
        COPYDATASTRUCT cds; cds.dwData = 1; cds.cbData = (DWORD)(filePath.length() + 1) * sizeof(wchar_t); cds.lpData = (PVOID)filePath.c_str();
        SendMessage(hExistingWnd, WM_COPYDATA, 0, (LPARAM)&cds);
      }
      else { if (IsIconic(hExistingWnd)) ShowWindow(hExistingWnd, SW_RESTORE); SetForegroundWindow(hExistingWnd); }
      if (argv) LocalFree(argv);
    }
    if (hMutex) CloseHandle(hMutex); return 0;
  }

  wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH); PathRemoveFileSpecW(exePath); SetCurrentDirectoryW(exePath);
  timeBeginPeriod(1);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); if (FAILED(hr)) return 0;
  if (!BASS_Init(-1, 48000, 0, nullptr, nullptr)) {}
  g_hFlacPlugin = BASS_PluginLoad("bassflac.dll", 0); BASS_PluginLoad("bass_aac.dll", 0);

  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dwriteFactory);
  hr = g_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"zh-cn", &g_textFormat);
  if (SUCCEEDED(hr)) { g_textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP); IDWriteInlineObject* pEllipsis = nullptr; g_dwriteFactory->CreateEllipsisTrimmingSign(g_textFormat, &pEllipsis); DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; g_textFormat->SetTrimming(&trimming, pEllipsis); SafeRelease(&pEllipsis); }
  hr = g_dwriteFactory->CreateTextFormat(L"Segoe MDL2 Assets", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"zh-cn", &g_textFormatIcon);
  if (SUCCEEDED(hr)) { g_textFormatIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_textFormatIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
  hr = g_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"zh-cn", &g_textFormatCenter);
  if (SUCCEEDED(hr)) { g_textFormatCenter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_textFormatCenter->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
  hr = g_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"zh-cn", &g_textFormatLyric);
  if (SUCCEEDED(hr)) { g_textFormatLyric->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_textFormatLyric->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); g_textFormatLyric->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP); IDWriteInlineObject* pEllipsis = nullptr; g_dwriteFactory->CreateEllipsisTrimmingSign(g_textFormatLyric, &pEllipsis); DWRITE_TRIMMING trimming = {}; trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER; trimming.delimiter = 0; trimming.delimiterCount = 0; g_textFormatLyric->SetTrimming(&trimming, pEllipsis); SafeRelease(&pEllipsis); }
  hr = g_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"zh-cn", &g_textFormatSmall);
  if (SUCCEEDED(hr)) { g_textFormatSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_textFormatSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
  hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wicFactory));

  WNDCLASSW wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.lpszClassName = L"D2D_A42_Wave"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
  RegisterClassW(&wc);
  int desiredLogicalW = 520, desiredLogicalH = 305;
  HDC hdc = GetDC(NULL); int dpiX = GetDeviceCaps(hdc, LOGPIXELSX), dpiY = GetDeviceCaps(hdc, LOGPIXELSY); ReleaseDC(NULL, hdc);
  int physicalW = MulDiv(desiredLogicalW, dpiX, 96), physicalH = MulDiv(desiredLogicalH, dpiY, 96);
  RECT rc = { 0, 0, physicalW, physicalH }; AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
  int winW = rc.right - rc.left, winH = rc.bottom - rc.top; int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
  g_hwnd = CreateWindowExW(0, L"D2D_A42_Wave", L"KeyPlayer\u3000", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, (screenW - winW) / 2, (screenH - winH) / 2, winW, winH, nullptr, nullptr, hInstance, nullptr);

  ShowWindow(g_hwnd, nCmdShow); SetTimer(g_hwnd, IDT_TIMER_PLAYCHECK, 500, nullptr); SetTimer(g_hwnd, IDT_TIMER_UI, 16, nullptr);
  int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv && argc > 1) {
    std::wstring filePath = argv[1]; wchar_t folderPath[MAX_PATH]; wcscpy_s(folderPath, filePath.c_str()); PathRemoveFileSpecW(folderPath);
    ScanFolderForAudio(folderPath);
    int targetIndex = 0; for (size_t i = 0; i < g_playlist.size(); i++) { if (g_playlist[i] == filePath) { targetIndex = (int)i; break; } }
    if (!g_playlist.empty()) PlayTrackByIndex(targetIndex, true);
  }
  else {
    wchar_t iniPath[MAX_PATH]; GetModuleFileNameW(NULL, iniPath, MAX_PATH); PathRemoveFileSpecW(iniPath); PathAppendW(iniPath, L"config.ini");
    wchar_t lastFolder[MAX_PATH] = { 0 }; GetPrivateProfileStringW(L"Settings", L"LastFolder", L"", lastFolder, MAX_PATH, iniPath);
    if (wcslen(lastFolder) > 0 && PathFileExistsW(lastFolder)) { ScanFolderForAudio(lastFolder); if (!g_playlist.empty()) PlayTrackByIndex(0, false); }
  }
  if (argv) LocalFree(argv);
  MSG msg = {}; while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  SafeRelease(&g_wicFactory); SafeRelease(&g_textFormatCenter); SafeRelease(&g_textFormat); SafeRelease(&g_textFormatIcon); SafeRelease(&g_textFormatSmall); SafeRelease(&g_textFormatLyric);
  SafeRelease(&g_dwriteFactory); SafeRelease(&g_d2dFactory); SafeRelease(&g_brushHighlight); SafeRelease(&g_brushDim); SafeRelease(&g_brushSpectrum);
  if (g_stream) BASS_StreamFree(g_stream); if (g_hFlacPlugin) BASS_PluginFree(g_hFlacPlugin); BASS_Free(); CoUninitialize();
  if (hMutex) CloseHandle(hMutex); timeEndPeriod(1); return 0;
}