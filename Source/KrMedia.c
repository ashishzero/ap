#include "KrMediaImpl.h"

inproc bool KrAudioIsPlayingFallback() { return false; }
inproc void KrAudioUpdateFallback() {}
inproc void KrAudioResumeFallback() {}
inproc void KrAudioPauseFallback() {}
inproc void KrAudioResetFallback() {}
inproc bool KrAudioSetRenderDeviceFallback(KrAudioDeviceId id) { return false; }
inproc uint KrAudioGetDeviceListFallback(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) { return 0; }
inproc bool KrAudioGetEffectiveDevice(KrAudioDeviceInfo *output) { return true; }
inproc bool KrWindowIsFullscreenFallback() { return false; }
inproc void KrWindowToggleFullscreenFallback() {}

inproc void KrEventFallback(const KrEvent *event, void *user) {}
inproc void KrUpdateFallback(void *user) {}
inproc u32  KrUploadAudioFallback(const KrAudioSpec *spec, u8 *data, u32 count, void *user) { return 0; }

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

proc bool KrWindow_IsFullscreen() {
	return g_Library.Window.IsFullscreen();
}

proc void KrWindow_ToggleFullscreen() {
	g_Library.Window.ToggleFullscreen();
}

proc bool KrAudio_IsPlaying() {
	return g_Library.Audio.IsPlaying();
}

proc void KrAudio_Update() {
	g_Library.Audio.Update();
}

proc void KrAudio_Resume() {
	g_Library.Audio.Resume();
}

proc void KrAudio_Pause() {
	g_Library.Audio.Pause();
}

proc void KrAudio_Reset() {
	g_Library.Audio.Reset();
}

proc bool KrAudio_SetRenderDevice(KrAudioDeviceId id) {
	return g_Library.Audio.SetRenderDevice(id);
}

proc uint KrAudio_GetDeviceList(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) {
	return g_Library.Audio.GetDeviceList(flow, inactive, output, cap);
}

proc bool KrAudio_GetEffectiveDevice(KrAudioDeviceInfo *output) {
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
