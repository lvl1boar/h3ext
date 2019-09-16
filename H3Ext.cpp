// H3Ext.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS

#define MVAR(type, varName, offset) inline type& varName(){return *reinterpret_cast<type*>(reinterpret_cast<unsigned char*>(this) + offset);}
#define SVAR(type, varName, loc) static inline type& varName(){ static unsigned long adr = 0; if(!adr) { adr = loc;} return *reinterpret_cast<type*>(adr);}
#define TARRAY(x, size) std::array<x, size>


#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <array>
#include <vector>
#include <d3d9.h>
#include <d3dx9.h>
#include <dwmapi.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib,"Dwmapi.lib")


//////////////////////////////////////////////////////////////////////////////////////
//	Process
//////////////////////////////////////////////////////////////////////////////////////
namespace process
{
DWORD getProcessId(const std::string& processName)
{
	DWORD processId = 0;

	HANDLE processSnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (processSnapshotHandle != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 processEntry;
		processEntry.dwSize = sizeof(processEntry);

		if (Process32First(processSnapshotHandle, &processEntry)) {
			do {
				if (processName == processEntry.szExeFile) {
					processId = processEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(processSnapshotHandle, &processEntry));
		}
		CloseHandle(processSnapshotHandle);
	}
	return processId;
}
BYTE* getModuleBaseAddress(const std::string& moduleName, int processId)
{
	BYTE* moduleBase = nullptr;
	MODULEENTRY32 moduleEntry;

	moduleEntry.dwSize = sizeof(moduleEntry);

	HANDLE moduleSnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
	if (moduleSnapshotHandle != INVALID_HANDLE_VALUE)
	{
		if (Module32First(moduleSnapshotHandle, &moduleEntry)) {
			do
			{
				if (moduleName == moduleEntry.szModule) {
					moduleBase = moduleEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(moduleSnapshotHandle, &moduleEntry));
		}
		CloseHandle(moduleSnapshotHandle);
	}
	return moduleBase;
}



class ExtMem
{
public:
	ExtMem()
	{

	}

	~ExtMem()
	{
		Detach();
	}

	bool Attach(const std::string& processName_)
	{
		if ((processId = getProcessId(processName_)) == 0)
			return false;
		if ((processHandle = OpenProcess(PROCESS_ALL_ACCESS, false, processId)) == INVALID_HANDLE_VALUE ||
			processHandle == NULL)
			return false;
		return true;
	}

	bool Detach()
	{
		if (processHandle != INVALID_HANDLE_VALUE)
			if (!CloseHandle(processHandle))
				return false;
		processId = 0;
		return true;
	}

	bool isAttached() const
	{
		return processHandle != INVALID_HANDLE_VALUE;
	}


	template <typename T> T Read(void* ptr) const {
		T t;
		ReadProcessMemory(processHandle, ptr, &t, sizeof(T), nullptr);
		return t;
	}

	bool Read(BYTE* ptr, size_t size, void* buf) const {
		return ReadProcessMemory(processHandle, ptr, buf, size, nullptr);
	}

	void Write(BYTE* ptr, size_t size, void* buf) const {
		WriteProcessMemory(processHandle, ptr, buf, size, nullptr);
	}

	BYTE* FindSignature(std::string moduleName, std::string signature, size_t offset = 0)
	{
		BYTE* returnValue = nullptr;

		std::vector<int> bytes;
		auto s = const_cast<char*>(signature.c_str());
		auto e = s + signature.length();
		for (auto c = s; c < e; c++) {
			if (*c == '?') {
				c++;
				if (*c == '?')
					c++;
				bytes.push_back(-1);
			}
			else
				bytes.push_back(strtoul(c, &c, 16));
		}

		auto moduleBase = getModuleBaseAddress(moduleName, processId);
		if (!moduleBase)
			return nullptr;

		const auto dosHeader = Read<IMAGE_DOS_HEADER>(moduleBase);
		if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
			return nullptr;

		const auto ntHeader = Read<IMAGE_NT_HEADERS>(reinterpret_cast<void*>(moduleBase + dosHeader.e_lfanew));
		if (ntHeader.Signature != IMAGE_NT_SIGNATURE)
			return nullptr;

		for (auto section = 0; section < ntHeader.FileHeader.NumberOfSections; section++)
		{
			const auto sectionHeader = Read<IMAGE_SECTION_HEADER>(
				moduleBase + dosHeader.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) +
				ntHeader.FileHeader.SizeOfOptionalHeader + (section * sizeof(IMAGE_SECTION_HEADER)));

			if (strcmp(reinterpret_cast<const char*>(sectionHeader.Name), ".text") == 0)
			{
				auto textSection = reinterpret_cast<BYTE*>(VirtualAlloc(nullptr, sectionHeader.Misc.VirtualSize, MEM_COMMIT, PAGE_READWRITE));
				if (!textSection)
					return nullptr;

				if (Read(moduleBase + sectionHeader.VirtualAddress, sectionHeader.Misc.VirtualSize, textSection)) {
					for (auto i = 0; i < sectionHeader.Misc.VirtualSize - bytes.size(); i++) {
						auto found = true;
						for (auto k = 0; k < bytes.size(); k++) {
							if (textSection[i + k] != bytes[k] && bytes[k] != -1) {
								found = false;
							}
						}
						if (found) {
							returnValue = reinterpret_cast<BYTE*>(i + moduleBase + sectionHeader.VirtualAddress + offset);
							break;
						}
					}
				}

				VirtualFree(textSection, sectionHeader.SizeOfRawData, MEM_FREE);
			}
		}

		return returnValue;
	}

private:
	HANDLE processHandle = INVALID_HANDLE_VALUE;
	DWORD processId = 0;
};
}

//////////////////////////////////////////////////////////////////////////////////////
//	Console
//////////////////////////////////////////////////////////////////////////////////////
class Console
{
public:

};

#define log(x) std::cout << x << std::endl;

//////////////////////////////////////////////////////////////////////////////////////
//	overlay
//////////////////////////////////////////////////////////////////////////////////////


namespace dxoverlay
{
class DX9Renderer
{
public:

};

class DX9Overlay
{
public:
	DX9Overlay(const std::string& targetWindowName_, HINSTANCE appInstance_, void render_(DX9Overlay*))
		: targetWndName(targetWindowName_), appInstance(appInstance_), render(render_)
	{
	}

	void drawString(const int x, const int y, D3DCOLOR color, const char* text, ...) const {
		va_list va_alist;
		char buf[1024];
		va_start(va_alist, text);
		vsprintf(buf, text, va_alist);
		va_end(va_alist);

		if (d3dFont)
		{
			RECT fontPos = { x, y, x + 200, y + 200 };
			d3dFont->DrawText(NULL, buf, -1, &fontPos, DT_NOCLIP, color);
		}
	}

	void drawLine(float x1, float y1, float x2, float y2, float width, bool antialias, DWORD color) const {
		D3DXVECTOR2 coords[] = { D3DXVECTOR2(x1, y1), D3DXVECTOR2(x2, y2) };
		ID3DXLine* line;

		D3DXCreateLine(d3dDevice, &line);
		line->SetWidth(width);
		line->SetAntialias(1);
		line->Begin();
		line->Draw(coords, 2, color);
		line->End();
		line->Release();
	}
	void drawFill(float x, float y, float w, float h, DWORD color) {
		D3DXVECTOR2 vLine[2];
		static ID3DXLine* line = nullptr;
		if (!line) D3DXCreateLine(d3dDevice, &line);

		line->SetWidth(w);
		line->SetAntialias(false);
		line->SetGLLines(true);

		vLine[0].x = x + w / 2;
		vLine[0].y = y;
		vLine[1].x = x + w / 2;
		vLine[1].y = y + h;

		line->Begin();
		line->Draw(vLine, 2, color);
		line->End();
	}

	void drawLine(float x, float y, float x1, float y1, DWORD color) {
		D3DXVECTOR2 vLine[2];
		static ID3DXLine* line = nullptr;
		if (!line) D3DXCreateLine(d3dDevice, &line);

		line->SetWidth(1);
		line->SetAntialias(false);
		line->SetGLLines(true);

		vLine[0].x = x;
		vLine[0].y = y;
		vLine[1].x = x1;
		vLine[1].y = y1;

		line->Begin();
		line->Draw(vLine, 2, color);
		line->End();
	}

	void drawOutline(float x, float y, float w, float h, DWORD color) {
		w -= 1; h -= 1;
		drawLine(x, y, x + w, y, color);
		drawLine(x + w, y, x + w, y + h, color);
		drawLine(x, y, x, y + h, color);
		drawLine(x, y + h, x + w, y + h, color);
	}


	bool run() {
		MSG msg;

		if (!createOverlayWindow()) return false;

		while (TRUE)
		{
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
			if (msg.message == WM_QUIT)	break;

			if (!updatePosition()) return false;
			if (!MoveWindow(wnd, getXPos(), getYPos(), getWidth(), getHeight(), true)) return false;

			d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
			d3dDevice->BeginScene();
			//if (GetForegroundWindow() == targetWnd) 
			render(this);
			d3dDevice->EndScene();
			d3dDevice->Present(NULL, NULL, NULL, NULL);

		}
		return true;
	}

	POINT getCursorPos() const {
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(targetWnd, &point);
		return point;
	}

	LPDIRECT3DDEVICE9 getDevice() const { return d3dDevice; }
	const int getWidth() const { return targetRect.right - targetRect.left; }
	const int getHeight() const { return targetRect.bottom - targetRect.top; }
	const int getXPos() const { return targetRect.left; }
	const int getYPos() const { return targetRect.top; }

private:

	bool updatePosition() {
		WINDOWINFO windowInfo;
		int borderX, borderY;

		ZeroMemory(&windowInfo, sizeof(windowInfo));
		borderX = borderY = 0;

		if (!GetWindowInfo(targetWnd, &windowInfo)) return false;
		DwmGetWindowAttribute(targetWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &targetRect, sizeof(targetRect));
		targetRect.top += windowInfo.rcClient.top - windowInfo.rcWindow.top;

		borderX = GetSystemMetrics(SM_CXBORDER);
		borderY = GetSystemMetrics(SM_CYBORDER);

		targetRect.top += borderY;
		//targetRect.bottom -= borderY;
		targetRect.left += borderX;
		targetRect.right -= borderX;

		return true;
	}

	bool createOverlayWindow()
	{
		WNDCLASSEX wc;
		D3DPRESENT_PARAMETERS d3dpp;
		MARGINS margins = { -1, -1, -1, -1 };

		ZeroMemory(&wc, sizeof(WNDCLASSEX));
		ZeroMemory(&d3dpp, sizeof(d3dpp));
		margins = { -1, -1, -1, -1 };

		if (!(targetWnd = FindWindow(nullptr, targetWndName.c_str()))) return false;
		if (!updatePosition()) return false;

		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = wndProc;
		wc.hInstance = appInstance;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.lpszClassName = "lvl1boar";
		if (!RegisterClassEx(&wc)) return false;

		if (!(wnd = CreateWindowEx(NULL, "lvl1boar", "lvl1boar", WS_EX_TOPMOST | WS_POPUP, getXPos(), getYPos(), getWidth(), getHeight(), NULL, NULL, appInstance, NULL))) return false;
		SetLayeredWindowAttributes(wnd, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
		if (!SetWindowPos(wnd, HWND_TOPMOST, getXPos(), getYPos(), 0, 0, SWP_NOSIZE)) return false;
		ShowWindow(wnd, SW_SHOWDEFAULT);
		SetWindowLong(wnd, GWL_EXSTYLE, GetWindowLong(wnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
		if (DwmExtendFrameIntoClientArea(wnd, &margins) != S_OK) return false;

		d3dpp.Windowed = TRUE;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.hDeviceWindow = wnd;
		d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
		d3dpp.BackBufferWidth = getWidth();
		d3dpp.BackBufferHeight = getHeight();
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		if (!(d3dInterface = Direct3DCreate9(D3D_SDK_VERSION))) return false;
		if (FAILED(d3dInterface->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &d3dDevice))) return false;
		D3DXCreateFont(d3dDevice, 14, 0, FW_ULTRALIGHT, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &d3dFont);
		return true;
	}


	static LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_PAINT:
		{
		}
		break;
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		} 
		break;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	RECT targetRect = {};
	HINSTANCE appInstance = NULL;
	HWND wnd = NULL, targetWnd = NULL;
	std::string targetWndName = "";
	LPDIRECT3D9 d3dInterface = nullptr;
	LPDIRECT3DDEVICE9 d3dDevice = nullptr;
	ID3DXFont* d3dFont = nullptr;
	void (*render)(DX9Overlay*) = nullptr;
};
}
//////////////////////////////////////////////////////////////////////
//	HOMM3 HOTA
//////////////////////////////////////////////////////////////////////
namespace h3hota
{
class AdvManager;
class MouseManager;
class HeroWindowManager;

#define MAX_PLAYERS 8
#define MAX_PLAYER_UNITS 16
#define INVALID_UNIT_INDEX 0xFFFFFFFF
#define TILE_DIMENSION 32

enum ResourceType {
	kWood,
	kMercury,
	kOre,
	kSulphur,
	kCrystals,
	kGems,
	kGold,
	kResourceMax
};
enum TileContentsId {
	kTileContentsResource = 79,
	kTileContentsArtifact = 5,
	kTileContentsMonster = 54,
	kTileContentsMountain = -122,
	kTileContentsStone = -116
};
enum TileTerrainType {
	kTerrainTypeUnk0,
	kTerrainTypeUnk1,
	kGrassLands,
	kTerrainTypeUnk3,
	kTerrainTypeUnk4,
	kTerrainTypeUnk5,
	kTerrainTypeUnk6,
	kTerrainTypeUnk7,
	kTerrainTypeUnk8,
};
enum TileMasks {
	kTileMaskUnoccupiable = (1 << 0),
	kTileMaskShore = (1 << 1),
	kUnk12 = (1 << 2),
	kUnk13 = (1 << 3),
	kTileMaskInteractable = (1 << 4),
	kUnk15 = (1 << 5),
	kUnk16 = (1 << 6),
	kUnk17 = (1 << 7),
	kUnk18 = (1 << 8)
};
enum PlayerDataFlags {
	kValidPlayer = (1 << 0)
};
//ResourceNames[Tile->NameIndex] when Tile->ContentsId == 79
const char* resourceNames[]
{
	"Wood",
	"Mercury",
	"Ore",
	"Sulphur",
	"Crystals",
	"Gems",
	"Gold",
	"Abandoned"
};

//ArtifactNames[Tile->NameIndex] when Tile->ContentsId == 5
const char* tileContentNames[]
{
	"Nothing",
	"",
	"Altar of sacrifice",
	"Anchor point",
	"Arena"
	// ............
};

const char* artifactNames[]
{
	"Spell book",
	"Spell scroll",
	"The Grail",
	"Catapult"
};

class H3String {
public:
	MVAR(const char*, stringBuffer, 0x4);
	MVAR(const char*, stringSize, 0x8);
	MVAR(const char*, bufferSize, 0xC);
private:
	static constexpr auto size = 0x10;
	char pad[size];
};
class TileData {
public:
	TileData() { contents() = 0; }

	MVAR(int, contents, 0x0);
	MVAR(int, terrainType, 0x4);
	MVAR(unsigned short, mask, 0xD);
	MVAR(unsigned short, contentsId, 0x1E);
	MVAR(unsigned short, nameIndex, 0x22);
private:
	static constexpr auto size = 0x26;
	char pad[size];
};
class MapData {
public:
	MapData(){}
	bool valid() { return this && mapDimension() > 0; }
	bool validTile(int x, int y) { return x >= 0 && y >= 0 && x < mapDimension() && y < mapDimension(); }
	//2D array of tiles
	MVAR(TileData*, tileData, 0xD0);
	//Map size = MapDimension^2
	MVAR(int, mapDimension, 0xD4);
private:
	static constexpr auto size = 0x100; // GUESS
	char pad[size];
};
class UnitData {
public:
	UnitData() {}
	MVAR(short, xCoord, 0x0);
	MVAR(short, yCoord, 0x2);
	MVAR(short, selectedXCoord, 0x35);
	MVAR(short, selectedYCoord, 0x39);
	MVAR(TARRAY(char, 64), name, 0x23);
	MVAR(short, movementPointsRemaining, 0x4D);
private:
	static constexpr auto size = 0x492;
	char pad[size];
};
class PlayerData {
public:
	PlayerData()
	{
		validPlayer() = false;
	}

	MVAR(unsigned char, playerIndex, 0x0);
	MVAR(unsigned char, validPlayer, 0x1);
	MVAR(int, currentActiveUnit, 0x4);
	MVAR(TARRAY(int, MAX_PLAYER_UNITS), unitIds, 0x8);
	MVAR(int, resourceBase, 0x9c);
	MVAR(TARRAY(char, 256), name, 0xCC);
private:
	static constexpr auto size = 0x168;
	char pad[size];
};
class GameState {
public:
	MVAR(int, mapDimensions, 0x1F884);
	MVAR(H3String, mapString, 0x1FB40);
	MVAR(H3String, mapDescriptionString, 0x1FB50);
	MVAR(MapData, mapData, 0x1FB70);
	MVAR(TARRAY(PlayerData, MAX_PLAYERS), playerData, 0x20AD0);
	MVAR(TARRAY(UnitData, MAX_PLAYER_UNITS), unitData, unitDataOffset);
	MVAR(int, activeUnitId, 0x21610);

	static size_t unitDataOffset; // MUST BE INITIALIZED BY USER
	static constexpr auto unitDataOffsetLoc = 0x419440;

private:
	static constexpr auto size = 0x4E7D0;
	char pad[size];
};
size_t GameState::unitDataOffset = 0;

class Movement {
public:
	MVAR(int, movementPointsRemaining, 0x8);
	MVAR(int, movementPointsCap, 0xc);
	MVAR(int, movementPointsBase, 0x10);
private:
};
class H3Manager {
public:
	MVAR(TARRAY(char, 256), parent, 0x4);
	MVAR(TARRAY(char, 256), name, 0x14);
};
class HeroWindowManager : public H3Manager {
public:
	MVAR(MouseManager, mouseMananger, 0x8);
};
class MouseManager : public H3Manager {
public:
	MVAR(int, totalTilesMoved, 0xD8);
};
class AdvManager : public H3Manager {
public:
	AdvManager() {}

	static const int screenCoordToMapCoord(int screenCoord) { 
		return static_cast<int>(screenCoord / TILE_DIMENSION);
	}

	static const int mapCoordToScreenCoord(int mapCoord) { 
		return static_cast<int>(mapCoord * TILE_DIMENSION); 
	}

	static const bool isValidTileCoordinate(int x, int y) { 
		const auto tileBounds = getTileBounds(0, 0); 
		return x >= 0 && x < tileBounds.x && y >= 0 && y < tileBounds.y; 
	}

	static const POINT getTileBounds(int resX, int resY) { 
		POINT p;
		if (resX == 1176 && resY == 664) { p.x = 31; p.y = 19; } //1176 x 664
		else { p.x = 36; p.y = 22; }
		return p; 
	}

	static const int normalizeTileCoordinate(int tileCoordinate) {
		if (tileCoordinate >= 1000) tileCoordinate -= 1024;
		int ddd = 0x3FF;
		return tileCoordinate;
	}

	MVAR(short, heroWindowManager, 0x8);
	MVAR(short, overviewTileX, 0xE4);
	MVAR(short, overviewTileY, 0xE6);
	MVAR(short, absTargetedMapTileX, 0xE8);
	MVAR(short, absTargetedMapTileY, 0xEA);
	MVAR(short, relTargetedMapTileX, 0xEC);
	MVAR(short, relTargetedMapTileY, 0xF0);
	MVAR(short, frameCount, 0x100);
	MVAR(MapData*, mapData, 0x5C);

private:
	static constexpr auto size = 0x3b8;
	char pad[size];
};
class Game {
public:
	SVAR(short, activePlayerId, 0x69CCF4);
	SVAR(short, fullScreenMode, 0x698A40);
	SVAR(short, networkGame, 0x69959C);
	SVAR(short, activeClient, 0x69CCFC);
	SVAR(GameState*, gameState, 0x699538);
	SVAR(AdvManager*, advManager, 0x6992B8);
	SVAR(TARRAY(char*, kResourceMax), resourceDescs, 0x6A5ECC); //contentsid 5
	SVAR(TARRAY(char*, 0x1000), buildingDescs, 0x006A7A64); //contentsid 4
	//arties .text:100F817D hota.dll

private:
};

class External
{
public:
	bool attach(){
		if (!mem.isAttached())
			if (!mem.Attach("h3hota.exe")) { log("[External::attach] attach failed"); return false; }
		return true;
	}

	PlayerData getPlayerData(const int index) const	{
		PlayerData playerData;
		GameState* gameState;

		if ((gameState = getGameStateLoc())) playerData = mem.Read<PlayerData>(&gameState->playerData().data()[index]);
		return playerData;
	}

	UnitData getUnitData(const int index) const	{
		UnitData unitData;
		GameState* gameState;

		GameState::unitDataOffset = mem.Read<int>(reinterpret_cast<void*>(GameState::unitDataOffsetLoc));
		if ((gameState = getGameStateLoc())) unitData = mem.Read<UnitData>(&gameState->unitData().data()[index]);
		return unitData;
	}

	MapData getMapData() const {
		MapData mapData;
		AdvManager* advManager;
		MapData* mapDataLoc;

		if ((advManager = getAdvManagerLoc())) {
			mapDataLoc = mem.Read<MapData*>(&advManager->mapData());
			if (mapDataLoc) mapData = mem.Read<MapData>(mapDataLoc);
		}
		return mapData;
	}

	TileData getTile(const int index) const {
		TileData tileData;
		MapData mapData;

		mapData = getMapData();
		if (mapData.valid()) tileData = mem.Read<TileData>(&mapData.tileData()[index]);
		return tileData;
	}

	TARRAY(char, 40) getResourceDesc(const int contentsId) {
		TARRAY(char, 40) tileDesc = { 0 };
		void* tileDescPtr;

		tileDescPtr = mem.Read<void*>(&Game::resourceDescs().data()[contentsId]);
		if (tileDescPtr) tileDesc = mem.Read<TARRAY(char, 40)>(tileDescPtr);
		return tileDesc;
	}

	int getActiveClient() const	{
		return mem.Read<short>(&Game::activeClient());
	}

	int generateRandomMapSeed() {
		SYSTEMTIME lt;
		TIME_ZONE_INFORMATION tzi;
		int genTzi = 0;
		int getTziRet = 0;
		DWORD genBias = 0;

		GetLocalTime(&lt);
		getTziRet = GetTimeZoneInformation(&tzi);

		if (getTziRet != -1) { genBias = 60 * tzi.Bias; if (tzi.StandardDate.wMonth) genBias = 60 * tzi.StandardBias + 60 * tzi.Bias; }
		if (genTzi != -1) genTzi = getTziRet == 2 && tzi.DaylightDate.wMonth && tzi.DaylightBias;

		//rip hota year 2038
		if ((lt.wYear - 1900 < 70) || (lt.wYear - 1900 > 138)) return -1;

		static constexpr auto daysPerYear = 365;
		int daysPassedAtMonth[]{ -1, 30, 58, 89, 119, 150, 180, 221, 242, 272, 303, 333, 364 };

		auto genDay = lt.wDay + daysPassedAtMonth[lt.wMonth];
		if (!((lt.wYear - 1900) & 3) && lt.wMonth > 2) genDay++;

		auto seed = genBias + 60 * (lt.wMinute + 60 * (lt.wHour + 24 * (((lt.wYear - 1901) >> 2) + genDay + 365 * (lt.wYear - 1900))))
			+ lt.wSecond + 0x7C558180;

		return seed;
	}

	AdvManager getAdvManager() const {
		return mem.Read<AdvManager>(getAdvManagerLoc());
	}
	
	GameState* getGameStateLoc() const {
		return mem.Read<GameState*>(&Game::gameState());
	}
	
	AdvManager* getAdvManagerLoc() const {
		return mem.Read<AdvManager*>(&Game::advManager());
	}

	MapData* getMapDataLoc() const {
		return mem.Read<MapData*>(&getAdvManagerLoc()->mapData());
	}
private:
	//Access to process memory
	process::ExtMem mem;
};
}

h3hota::External* hota;
dxoverlay::DX9Overlay* overlay;

void renderFrame(dxoverlay::DX9Overlay* overlay)
{
	static auto white = D3DCOLOR_RGBA(255, 255, 255, 255);
	static auto yellow = D3DCOLOR_RGBA(255, 255, 0, 255);
	static auto blue = D3DCOLOR_RGBA(0, 0, 255, 255);
	static auto opaqueBlue = D3DCOLOR_RGBA(0, 0, 255, 100);
	static auto red = D3DCOLOR_RGBA(255, 0, 0, 255);

	int lineIndex = 0;
	auto mapData = hota->getMapData();
	auto advManager = hota->getAdvManager();


	auto drawGrid = [&](int infoLevel = 0)
	{
		//overlay->drawOutline(0, 0, overlay->getWidth(), overlay->getHeight(), yellow);
		static auto gridColor = D3DCOLOR_RGBA(0, 255, 0, 50);
		static auto resourceColor = D3DCOLOR_RGBA(0, 255, 0, 100);
		static auto artifactColor = D3DCOLOR_RGBA(255, 200, 0, 100);
		static auto monsterColor = D3DCOLOR_RGBA(255, 0, 0, 100);


		if (hota->getActiveClient() > 0)
		{
			auto mapAreaX = 0, mapAreaY = 0;
			overlay->drawOutline(mapAreaX, mapAreaY, overlay->getWidth(), overlay->getHeight(), yellow);
			for (auto x = 0; x < mapData.mapDimension(); x++) {
				for (auto y = 0; y < mapData.mapDimension(); y++) {
					if (advManager.isValidTileCoordinate(x,y)) {
						overlay->drawOutline(advManager.mapCoordToScreenCoord(x), advManager.mapCoordToScreenCoord(y), TILE_DIMENSION, TILE_DIMENSION, gridColor);

						const auto ovTileX = x + advManager.normalizeTileCoordinate(advManager.overviewTileX());
						const auto ovTileY = y + advManager.normalizeTileCoordinate(advManager.overviewTileY());

						if (mapData.validTile(ovTileX, ovTileY)) {
							auto tileData = hota->getTile(ovTileX + (mapData.mapDimension() * ovTileY));

							auto color = D3DCOLOR_RGBA(100, 100, 100, 200);
							if (tileData.contentsId() == h3hota::kTileContentsResource) color = resourceColor;
							if (tileData.contentsId() == h3hota::kTileContentsMonster) color = monsterColor;
							if (tileData.contentsId() == h3hota::kTileContentsArtifact) color = artifactColor;

							if(tileData.contentsId() != 0)
								overlay->drawFill(advManager.mapCoordToScreenCoord(x), advManager.mapCoordToScreenCoord(y), TILE_DIMENSION, TILE_DIMENSION, color);
						}
					}
				}
			}
		}
	};

	auto drawCursorTile = [&]()
	{
		const auto cursorPos = overlay->getCursorPos();
		const auto tilePosX = advManager.screenCoordToMapCoord(cursorPos.x);
		const auto tilePosY = advManager.screenCoordToMapCoord(cursorPos.y);

		if (advManager.isValidTileCoordinate(tilePosX, tilePosY)) {
			overlay->drawFill(advManager.mapCoordToScreenCoord(tilePosX), advManager.mapCoordToScreenCoord(tilePosY),
				TILE_DIMENSION, TILE_DIMENSION, opaqueBlue);
		}
	};

	auto drawPlayerInfo = [&]()
	{
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "Active client %d", hota->getActiveClient());

		for (auto i = 0; i < MAX_PLAYERS; i++) {
			auto playerData = hota->getPlayerData(i);
			if (playerData.validPlayer()) {
				overlay->drawString(30, 40 + (15 * lineIndex++), white, "--------");
				overlay->drawString(30, 40 + (15 * lineIndex++), white, "Player: %d", i);
				//overlay->drawString(30, 40 + (15 * lineIndex++), white, "PlayerName: %s", playerData.name().data());
				for (auto k = 0; k < MAX_PLAYER_UNITS; k++) {
					if (playerData.unitIds()[k] == INVALID_UNIT_INDEX) break;
					auto unitData = hota->getUnitData(playerData.unitIds()[k]);
					overlay->drawString(30, 40 + (15 * lineIndex++), white, "Unit: %s [%d][%d][%d]", unitData.name().data(), playerData.unitIds()[k], unitData.xCoord(), unitData.yCoord());

				}
			}
		}
	};

	auto drawWindowInfo = [&]()
	{
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "CTX %d", advManager.screenCoordToMapCoord(overlay->getCursorPos().x));
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "CTY %d", advManager.screenCoordToMapCoord(overlay->getCursorPos().y));
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "WW %d", overlay->getWidth());
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "WH %d", overlay->getHeight());
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "CX %d", overlay->getCursorPos().x);
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "CY %d", overlay->getCursorPos().y);
	};

	auto drawSideBar = [&]()
	{
		static auto h = 550, w = 175;
		overlay->drawFill(20, 20, w, h, D3DCOLOR_RGBA(0, 0, 0, 220));
		overlay->drawOutline(20, 20, w, h, D3DCOLOR_RGBA(255, 255, 255, 255));
	};

	auto drawAddresses = [&]()
	{
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "gameState %lx", hota->getGameStateLoc());
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "advManager %lx", hota->getAdvManagerLoc());
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "advManager %lx", hota->getAdvManagerLoc());
		overlay->drawString(30, 40 + (15 * lineIndex++), white, "mapData %lx", hota->getMapDataLoc());

	};

	drawGrid();
	drawCursorTile();
	drawSideBar();
	drawAddresses();
	drawPlayerInfo();
	//h3hota::AdvManager advManager;
	//if (hota->getAdvManager(advManager))
	//{
	//	overlay->drawString(30, 40 + (15 * lineIndex++), white, "OX %d", advManager.overviewTileX());
	//	overlay->drawString(30, 40 + (15 * lineIndex++), white, "OY %d", advManager.overviewTileY());
	//	overlay->drawString(30, 40 + (15 * lineIndex++), white, "AX %d", advManager.absTargetedMapTileX());
	//	overlay->drawString(30, 40 + (15 * lineIndex++), white, "AY %d", advManager.absTargetedMapTileY());
	//	overlay->drawString(30, 40 + (15 * lineIndex++), white, "RX %d", advManager.relTargetedMapTileX());
	//	overlay->drawString(30, 40 + (15 * lineIndex++), white, "RY %d", advManager.relTargetedMapTileY());
	//
	//}

}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	hota = new h3hota::External;
	if (!hota->attach()) return -1;

	overlay = new dxoverlay::DX9Overlay("Heroes of Might and Magic III: Horn of the Abyss", hInstance, renderFrame);
	if (!overlay->run()) return -1;

	return 0;
}
