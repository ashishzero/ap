#include "KrMediaImpl.h"

static bool KrAudioIsPlayingFallback() { return false; }
static void KrAudioUpdateFallback() {}
static void KrAudioResumeFallback() {}
static void KrAudioPauseFallback() {}
static void KrAudioResetFallback() {}
static bool KrAudioSetRenderDeviceFallback(KrAudioDeviceId id) { return false; }
static uint KrAudioGetDeviceListFallback(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) { return 0; }
static bool KrAudioGetEffectiveDevice(KrAudioDeviceInfo *output) { return true; }
static bool KrWindowIsFullscreenFallback() { return false; }
static void KrWindowToggleFullscreenFallback() {}

static void KrEventFallback(const KrEvent *event, void *user) {}
static void KrUpdateFallback(float w, float h, void *user) {}
static u32  KrUploadAudioFallback(const KrAudioSpec *spec, u8 *data, u32 count, void *user) { return 0; }

const KrLibrary LibraryFallback = {
	.Audio = {
		.IsPlaying          = KrAudioIsPlayingFallback,
		.Update             = KrAudioUpdateFallback,
		.Resume             = KrAudioResumeFallback,
		.Pause              = KrAudioPauseFallback,
		.Reset              = KrAudioResetFallback,
		.SetRenderDevice    = KrAudioSetRenderDeviceFallback,
		.GetDeviceList      = KrAudioGetDeviceListFallback,
		.GetEffectiveDevice = KrAudioGetEffectiveDevice,
	},
	.Window = {
		.IsFullscreen     = KrWindowIsFullscreenFallback,
		.ToggleFullscreen = KrWindowToggleFullscreenFallback,
	}
};

KrLibrary g_Library = {
	.Audio = {
		.IsPlaying          = KrAudioIsPlayingFallback,
		.Update             = KrAudioUpdateFallback,
		.Resume             = KrAudioResumeFallback,
		.Pause              = KrAudioPauseFallback,
		.Reset              = KrAudioResetFallback,
		.SetRenderDevice    = KrAudioSetRenderDeviceFallback,
		.GetDeviceList      = KrAudioGetDeviceListFallback,
		.GetEffectiveDevice = KrAudioGetEffectiveDevice,
	},
	.Window = {
		.IsFullscreen     = KrWindowIsFullscreenFallback,
		.ToggleFullscreen = KrWindowToggleFullscreenFallback,
	}
};

KrUserContext g_UserContext = {
	.Data          = nullptr,
	.OnEvent       = KrEventFallback,
	.OnUpdate      = KrUpdateFallback,
	.OnUploadAudio = KrUploadAudioFallback,
};

bool KrWindow_IsFullscreen() {
	return g_Library.Window.IsFullscreen();
}

void KrWindow_ToggleFullscreen() {
	g_Library.Window.ToggleFullscreen();
}

bool KrAudio_IsPlaying() {
	return g_Library.Audio.IsPlaying();
}

void KrAudio_Update() {
	g_Library.Audio.Update();
}

void KrAudio_Resume() {
	g_Library.Audio.Resume();
}

void KrAudio_Pause() {
	g_Library.Audio.Pause();
}

void KrAudio_Reset() {
	g_Library.Audio.Reset();
}

bool KrAudio_SetRenderDevice(KrAudioDeviceId id) {
	return g_Library.Audio.SetRenderDevice(id);
}

uint KrAudio_GetDeviceList(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) {
	return g_Library.Audio.GetDeviceList(flow, inactive, output, cap);
}

bool KrAudio_GetEffectiveDevice(KrAudioDeviceInfo *output) {
	return g_Library.Audio.GetEffectiveDevice(output);
}

const char *KrEvent_GetName(KrEventKind kind) {
	static const char *EventNames[] = {
		[KrEvent_Startup]                    = "Startup",
		[KrEvent_Quit]                       = "Quit",
		[KrEvent_WindowCreated]              = "WindowCreated",
		[KrEvent_WindowDestroyed]            = "WindowClosed",
		[KrEvent_WindowActivated]            = "WindowActivated",
		[KrEvent_WindowDeactivated]          = "WindowDeactivated",
		[KrEvent_WindowResized]              = "WindowResized",
		[KrEvent_MouseMoved]                 = "MouseMoved",
		[KrEvent_ButtonPressed]              = "ButtonPressed",
		[KrEvent_ButtonReleased]             = "ButtonReleased",
		[KrEvent_DoubleClicked]              = "DoubleClicked",
		[KrEvent_WheelMoved]                 = "WheenMoved",
		[KrEvent_KeyPressed]                 = "KeyPressed",
		[KrEvent_KeyReleased]                = "KeyReleased",
		[KrEvent_TextInput]                  = "TextInput",
		[KrEvent_AudioResumed]               = "AudioResumed",
		[KrEvent_AudioPaused]                = "AudioPaused",
		[KrEvent_AudioReset]                 = "AudioReset",
		[KrEvent_AudioRenderDeviceChanged]   = "AudioRenderDeviceChanged",
		[KrEvent_AudioRenderDeviceLost]      = "AudioRenderDeviceLost",
		[KrEvent_AudioRenderDeviceActived]   = "AudioRenderDeviceActived",
		[KrEvent_AudioRenderDeviceDeactived] = "AudioRenderDeviceDeactived",
	};
	static_assert(ArrayCount(EventNames) == KrEvent_EnumMax, "");
	return EventNames[kind];
}
