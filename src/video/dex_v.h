#ifndef VIDEO_DEX_H
#define VIDEO_DEX_H
#pragma once

#include "video_driver.hpp"

class VideoDriver_Dex : public VideoDriver
{
public:
	std::string_view GetName() const override { return "DEX"; }

	VideoDriver_Dex(bool uses_hardware_acceleration = true);
	virtual ~VideoDriver_Dex(void);

	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	bool ClaimMousePointer() override;

	void EditBoxGainedFocus() override;

	void EditBoxLostFocus() override;

	std::vector<int> GetListOfMonitorRefreshRates() override;

	std::string_view GetInfoString() const override { return this->driver_info; }

protected:

	Palette local_palette{}; ///< Current palette to use for drawing.
	bool buffer_locked = false; ///< Video buffer was locked by the main thread.
	Rect dirty_rect{}; ///< Rectangle encompassing the dirty area of the video buffer.
	std::string driver_info{}; ///< Information string about selected driver.
	Dimension current_screen_size;

	int m_videoWidth = 0;
	int m_videoHeight = 0;
	int m_videoBPP = 0;
	void* m_videoPtr = nullptr;

	int m_mouseDownFlags = 0;
	int m_mouseUpFlags = 0;

	Dimension GetScreenSize() const override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	void CheckPaletteAnim() override;
	bool PollEvent() override;
	void Paint() override;

	/** Indicate to the driver the client-side might have changed. */
	void ClientSizeChanged(int w, int h, bool force);

	/** Create the main window. */
	bool CreateFramebuffer(uint w, uint h);

	bool AllocateBackingStore(int w, int h, bool force = false);

private:
	void LoopOnce();
	std::optional<std::string_view> Initialize();

	void UpdatePalette();
	void MakePalette();

	void OnMouseDown(int button);
	void OnMouseUp(int button);

	/* Convert a constant pointer back to a non-constant pointer to a member function. */
	static void EmscriptenLoop(void *self) { ((VideoDriver_Dex*)self)->LoopOnce(); }
};

/** Factory for the SDL video driver. */
class FVideoDriver_Dex : public DriverFactoryBase {
public:
	FVideoDriver_Dex() : DriverFactoryBase(Driver::DT_VIDEO, 5, "dex", "DEX Video Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<VideoDriver_Dex>(); }
};

#endif // VIDEO_DEX_H