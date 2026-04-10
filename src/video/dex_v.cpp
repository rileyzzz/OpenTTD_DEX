
#include "../stdafx.h"
#include "../openttd.h"
#include "../error_func.h"
#include "../gfx_func.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread.h"
#include "../progress.h"
#include "../core/geometry_func.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "dex_v.h"
#include <dex.h>

static FVideoDriver_Dex iFVideoDriver_Dex;


VideoDriver_Dex::VideoDriver_Dex(bool uses_hardware_acceleration)
	: VideoDriver(uses_hardware_acceleration)
{
}


VideoDriver_Dex::~VideoDriver_Dex(void)
{
	if (m_videoPtr)
	{
		free(m_videoPtr);
		m_videoPtr = nullptr;
	}
}
	
std::optional<std::string_view> VideoDriver_Dex::Start(const StringList &param)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) return "Only real blitters supported";

	auto error = this->Initialize();
	if (error) return error;

	// Ensure cursor isn't locked.
	DEX_ASM({
		OpenTTD.ModuleComponent.MouseCursorVisible = true;
	});

	current_screen_size = GetScreenSize();

	if (!CreateFramebuffer(current_screen_size.width, current_screen_size.height))
	{
		return "Failed to create framebuffer.";
	}

	this->driver_info = this->GetName();
	MarkWholeScreenDirty();

	// SDL_StopTextInput();
	// this->edit_box_focused = false;
	this->is_game_threaded = false;

	return std::nullopt;
}

std::optional<std::string_view> VideoDriver_Dex::Initialize()
{
	// this->UpdateAutoResolution();
	return std::nullopt;
}

void VideoDriver_Dex::Stop()
{
	DEX_ASM({
		OpenTTD.ModuleComponent.VideoShutdown();
	});
}

/** Indicate to the driver the client-side might have changed. */
void VideoDriver_Dex::ClientSizeChanged(int w, int h, bool force)
{
	/* Allocate backing store of the new size. */
	if (this->AllocateBackingStore(w, h, force)) {
		CopyPalette(this->local_palette, true);

		BlitterFactory::GetCurrentBlitter()->PostResize();

		GameSizeChanged();
	}
}

/** Create the main window. */
bool VideoDriver_Dex::CreateFramebuffer(uint w, uint h)
{
	DEX_ASM_ARGS({
		OpenTTD.ModuleComponent.VideoInit($0, $1);
	}, w, h);

	this->ClientSizeChanged(w, h, true);

	_cursor.in_window = true;
	return true;
}


void VideoDriver_Dex::UpdatePalette()
{
	uint32_t pal[256];
	// SDL_Color pal[256];

	for (int i = 0; i != this->local_palette.count_dirty; i++) {
		pal[i] = this->local_palette.palette[this->local_palette.first_dirty + i].data;
		// pal[i].r = this->local_palette.palette[this->local_palette.first_dirty + i].r;
		// pal[i].g = this->local_palette.palette[this->local_palette.first_dirty + i].g;
		// pal[i].b = this->local_palette.palette[this->local_palette.first_dirty + i].b;
		// pal[i].a = 0;
	}

	DEX_ASM_ARGS({
		var pal = Mem.Range($0, $2 * 4);
		OpenTTD.ModuleComponent.UpdatePalette(pal, $1);
	}, pal, this->local_palette.first_dirty, this->local_palette.count_dirty);
}

void VideoDriver_Dex::MakePalette()
{
	CopyPalette(this->local_palette, true);
	this->UpdatePalette();
}

void VideoDriver_Dex::Paint() 
{
	// PerformanceMeasurer framerate(PFE_VIDEO);

	if (IsEmptyRect(this->dirty_rect) && this->local_palette.count_dirty == 0) return;

	if (this->local_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PaletteAnimation::VideoBackend:
				this->UpdatePalette();
				break;

			case Blitter::PaletteAnimation::Blitter: {
				blitter->PaletteAnimate(this->local_palette);
				break;
			}

			case Blitter::PaletteAnimation::None:
				break;

			default:
				NOT_REACHED();
		}
		this->local_palette.count_dirty = 0;
	}

	DEX_ASM_ARGS({
		var video = Mem.Range($0, $2 * $3);
		OpenTTD.ModuleComponent.Blit(video, $1, $2, $4, $5, $6, $7);
	}, m_videoPtr, m_videoBPP, m_videoWidth * (m_videoBPP / 8), m_videoHeight, this->dirty_rect.left, this->dirty_rect.top, this->dirty_rect.right - this->dirty_rect.left, this->dirty_rect.bottom - this->dirty_rect.top);

	/*
	SDL_Rect r = { this->dirty_rect.left, this->dirty_rect.top, this->dirty_rect.right - this->dirty_rect.left, this->dirty_rect.bottom - this->dirty_rect.top };

	if (_sdl_surface != _sdl_real_surface) {
		SDL_BlitSurface(_sdl_surface, &r, _sdl_real_surface, &r);
	}
	SDL_UpdateWindowSurfaceRects(this->sdl_window, &r, 1);
	*/

	this->dirty_rect = {};
}


bool VideoDriver_Dex::AllocateBackingStore(int w, int h, bool force)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	if (!force && w == m_videoWidth && h == m_videoHeight) return false;

	/* Free any previously allocated rgb surface. */
	if (m_videoPtr != nullptr) {
		free(m_videoPtr);
		m_videoPtr = nullptr;
	}

	size_t pitch = w * (bpp / 8);
	m_videoPtr = malloc(pitch * h);
	m_videoWidth = w;
	m_videoHeight = h;
	m_videoBPP = bpp;

	/* X11 doesn't appreciate it if we invalidate areas outside the window
	 * if shared memory is enabled (read: it crashes). So, as we might have
	 * gotten smaller, reset our dirty rects. GameSizeChanged() a bit lower
	 * will mark the whole screen dirty again anyway, but this time with the
	 * new dimensions. */
	this->dirty_rect = {};

	_screen.width = w;
	_screen.height = h;
	_screen.pitch = pitch / (bpp / 8);
	_screen.dst_ptr = m_videoPtr;

	this->MakePalette();

	return true;
}


void VideoDriver_Dex::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	this->dirty_rect = BoundingRect(this->dirty_rect, r);
}


void VideoDriver_Dex::LoopOnce()
{
	if (_exit_game) {
		/* Emscripten is event-driven, and as such the main loop is inside
		 * the browser. So if _exit_game goes true, the main loop ends (the
		 * cancel call), but we still have to call the cleanup that is
		 * normally done at the end of the main loop for non-Emscripten.
		 * After that, Emscripten just halts, and the HTML shows a nice
		 * "bye, see you next time" message. */
		extern void PostMainLoop();
		PostMainLoop();

		dex_set_main_loop(nullptr, 0, 1);

		#if DEX_TODO
		dex_cancel_main_loop();
		emscripten_exit_pointerlock();

		/* In effect, the game ends here. As emscripten_set_main_loop() caused
		 * the stack to be unwound, the code after MainLoop() in
		 * openttd_main() is never executed. */
		if (_game_mode == GM_BOOTSTRAP) {
			EM_ASM(if (window["openttd_bootstrap_reload"]) openttd_bootstrap_reload());
		} else {
			EM_ASM(if (window["openttd_exit"]) openttd_exit());
		}
		#endif // DEX_TODO
		return;
	}

	this->Tick();
}


void VideoDriver_Dex::MainLoop()
{
	/* Run the main loop event-driven, based on RequestAnimationFrame. */
	dex_set_main_loop_arg(&this->EmscriptenLoop, this, 0, 1);

	DEX_ASM_ARGS({
		int fnMouseDown = Mem.LoadInt($1);
		int fnMouseUp = Mem.LoadInt($2);

		OpenTTD.ModuleComponent.MouseDownCallback = (int btn) => {
			Mem.CallFuncPtr(fnMouseDown, $0, btn);
		};

		OpenTTD.ModuleComponent.MouseUpCallback = (int btn) => {
			Mem.CallFuncPtr(fnMouseUp, $0, btn);
		};
	}, this, &VideoDriver_Dex::OnMouseDown, &VideoDriver_Dex::OnMouseUp);
}

bool VideoDriver_Dex::ChangeResolution(int w, int h)
{
	return CreateFramebuffer(w, h);
}

bool VideoDriver_Dex::ToggleFullscreen(bool fullscreen)
{
	// This is up to the game.
	return false;
}

bool VideoDriver_Dex::AfterBlitterChange()
{
	assert(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 0);

	current_screen_size = GetScreenSize();
	return CreateFramebuffer(current_screen_size.width, current_screen_size.height);
}

bool VideoDriver_Dex::ClaimMousePointer()
{
	// DEX_ASM({
	// 	Sandbox.Input.Input.MouseCursorVisible = false;
	// });

	return true;
}

void VideoDriver_Dex::EditBoxGainedFocus()
{

}

void VideoDriver_Dex::EditBoxLostFocus()
{

}

std::vector<int> VideoDriver_Dex::GetListOfMonitorRefreshRates()
{
	return { 60 };
}


Dimension VideoDriver_Dex::GetScreenSize() const
{
	uint w = 0, h = 0;
	DEX_ASM_ARGS({
		Mem.Store($0, (int)Sandbox.Screen.Size.x);
		Mem.Store($1, (int)Sandbox.Screen.Size.y);
	}, &w, &h);
	
	w = std::max(w, 192u);
	h = std::max(h, 108u);

	return { w, h };
}


void VideoDriver_Dex::InputLoop()
{
	// Looks like this is mainly for keyboard modifiers.
}


void VideoDriver_Dex::OnMouseDown(int button)
{
	m_mouseDownFlags |= button;
}


void VideoDriver_Dex::OnMouseUp(int button)
{
	m_mouseUpFlags |= button;
}


bool VideoDriver_Dex::PollEvent()
{
	// Handle events.
	// Dimension screenSize = GetScreenSize();
	// if (screenSize != current_screen_size)
	// {
	// 	current_screen_size = screenSize;
	// 	CreateFramebuffer(current_screen_size.width, current_screen_size.height);
	// }

	float mx = 0.f;
	float my = 0.f;

	DEX_ASM_ARGS({
		var mousePos = Sandbox.Mouse.Position;
		Mem.Store($0, mousePos.x);
		Mem.Store($1, mousePos.y);
	}, &mx, &my);

	if (_cursor.UpdateCursorPosition((int)mx, (int)my)) {
		// Warp the mouse position.
		mx = _cursor.pos.x;
		my = _cursor.pos.y;
		DEX_ASM_ARGS({
			Sandbox.Mouse.Position = new Vector2(Mem.LoadSingle($0), Mem.LoadSingle($1));
		}, &mx, &my);
	}
	HandleMouseEvents();
	
	float wheelX = 0.f;
	float wheelY = 0.f;
	DEX_ASM_ARGS({
		Mem.Store($0, (float)Sandbox.Input.MouseWheel.x);
		Mem.Store($1, (float)Sandbox.Input.MouseWheel.y);
	}, &wheelX, &wheelY);

	if (wheelX != 0.f || wheelY != 0.f) {
		if (wheelY > 0) {
			_cursor.wheel--;
		} else if (wheelY < 0) {
			_cursor.wheel++;
		}

		/* Handle 2D scrolling. */
		const float SCROLL_BUILTIN_MULTIPLIER = 14.0f;
		_cursor.v_wheel -= static_cast<float>(wheelY * SCROLL_BUILTIN_MULTIPLIER * _settings_client.gui.scrollwheel_multiplier);
		_cursor.h_wheel += static_cast<float>(wheelX * SCROLL_BUILTIN_MULTIPLIER * _settings_client.gui.scrollwheel_multiplier);

		_cursor.wheel_moved = true;
		HandleMouseEvents();
	}


	if (m_mouseDownFlags & 0x1) {
		_left_button_down = true;
		HandleMouseEvents();
	}
	if (m_mouseDownFlags & 0x2) {
		_right_button_down = true;
		_right_button_clicked = true;
		HandleMouseEvents();
	}

	if (m_mouseUpFlags & 0x1) {
		_left_button_down = false;
		_left_button_clicked = false;
		HandleMouseEvents();
	}

	if (m_mouseUpFlags & 0x2) {
		_right_button_down = false;
		HandleMouseEvents();
	}
	m_mouseDownFlags = 0;
	m_mouseUpFlags = 0;

	return false;
}


bool VideoDriver_Dex::LockVideoBuffer()
{
	if (this->buffer_locked) return false;
	this->buffer_locked = true;

	// _screen.dst_ptr = this->GetVideoPointer();
	_screen.dst_ptr = m_videoPtr;
	assert(_screen.dst_ptr != nullptr);

	return true;
}

void VideoDriver_Dex::UnlockVideoBuffer()
{
	if (_screen.dst_ptr != nullptr) {
		/* Hand video buffer back to the drawing backend. */
		// this->ReleaseVideoPointer();
		_screen.dst_ptr = nullptr;
	}

	this->buffer_locked = false;
}

void VideoDriver_Dex::CheckPaletteAnim()
{
	if (!CopyPalette(this->local_palette)) return;
	this->MakeDirty(0, 0, _screen.width, _screen.height);
}

