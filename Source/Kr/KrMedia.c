#include "KrMediaInternal.h"

static bool PL_Media_Fallback_IsFullscreen()                               { return false; }
static void PL_Media_Fallback_ToggleFullscreen()                           { }
static bool PL_Media_Fallback_IsAudioRendering()                           { return false; }
static void PL_Media_Fallback_UpdateAudioRender()                          {}
static void PL_Media_Fallback_PauseAudioRender()                           {}
static void PL_Media_Fallback_ResumeAudioRender()                          {}
static void PL_Media_Fallback_ResetAudioRender()                           {}
static bool PL_Media_Fallback_IsAudioCapturing()                           { return false; }
static void PL_Media_Fallback_PauseAudioCapture()                          {}
static void PL_Media_Fallback_ResumeAudioCapture()                         {}
static void PL_Media_Fallback_ResetAudioCapture()                          {}
static void PL_Media_Fallback_SetAudioDevice(PL_AudioDevice const *device) {}

static void PL_User_Fallback_OnEvent(PL_Event const *event, void *data)                               {}
static void PL_User_Fallback_OnUpdate(PL_IoDevice const *io, void *data)                              {}
static u32  PL_User_Fallback_OnAudioRender(PL_AudioSpec const *spec, u8 *out, u32 frames, void *data) { return 0; }
static u32  PL_User_Fallback_OnAudioCapture(PL_AudioSpec const *spec, u8 *in, u32 frames, void *data) { return frames; }

PL_Media Media = {
	.VTable = {
		.IsFullscreen         = PL_Media_Fallback_IsFullscreen,
		.ToggleFullscreen     = PL_Media_Fallback_ToggleFullscreen,
		.IsAudioRendering     = PL_Media_Fallback_IsAudioRendering,
		.UpdateAudioRender    = PL_Media_Fallback_UpdateAudioRender,
		.PauseAudioRender     = PL_Media_Fallback_PauseAudioRender,
		.ResumeAudioRender    = PL_Media_Fallback_ResumeAudioRender,
		.ResetAudioRender     = PL_Media_Fallback_ResetAudioRender,
		.IsAudioCapturing     = PL_Media_Fallback_IsAudioCapturing,
		.PauseAudioCapture    = PL_Media_Fallback_PauseAudioCapture,
		.ResumeAudioCapture   = PL_Media_Fallback_ResumeAudioCapture,
		.ResetAudioCapture    = PL_Media_Fallback_ResetAudioCapture,
		.SetAudioDevice       = PL_Media_Fallback_SetAudioDevice,
	},
	.UserVTable = {
		.OnEvent              = PL_User_Fallback_OnEvent,
		.OnUpdate             = PL_User_Fallback_OnUpdate,
		.OnAudioRender        = PL_User_Fallback_OnAudioRender,
		.OnAudioCapture       = PL_User_Fallback_OnAudioCapture,
	}
};

void PL_SetConfig(PL_Config const *config) {
	Media.Config = *config;

	if (!Media.Config.User.OnEvent)
		Media.Config.User.OnEvent = PL_User_Fallback_OnEvent;

	if (!Media.Config.User.OnUpdate)
		Media.Config.User.OnUpdate = PL_User_Fallback_OnUpdate;

	if (!Media.Config.User.OnAudioRender)
		Media.Config.User.OnAudioRender = PL_User_Fallback_OnAudioRender;
}

bool PL_IsFullscreen(void) {
	return Media.VTable.IsFullscreen();
}

void PL_ToggleFullscreen(void) {
	Media.VTable.ToggleFullscreen();
}

bool PL_IsAudioRendering(void) {
	return Media.VTable.IsAudioRendering();
}

void PL_UpdateAudioRender(void) {
	Media.VTable.UpdateAudioRender();
}

void PL_PauseAudioRender(void) {
	Media.VTable.PauseAudioRender();
}

void PL_ResumeAudioRender(void) {
	Media.VTable.ResumeAudioRender();
}

void PL_ResetAudioRender(void) {
	Media.VTable.ResetAudioRender();
}

void PL_SetAudioDevice(PL_AudioDevice const *device) {
	Media.VTable.SetAudioDevice(device);
}

bool PL_IsAudioCapturing(void) {
	return Media.VTable.IsAudioCapturing();
}

void PL_PauseAudioCapture(void) {
	Media.VTable.PauseAudioCapture();
}

void PL_ResumeAudioCapture(void) {
	Media.VTable.ResumeAudioCapture();
}

void PL_ResetAudioCapture(void) {
	Media.VTable.ResetAudioCapture();
}
