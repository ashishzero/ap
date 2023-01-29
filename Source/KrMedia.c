#include "KrMediaImpl.h"

inproc bool KrAudioIsPlayingFallback() { return false; }
inproc void KrAudioUpdateFallback() {}
inproc void KrAudioResumeFallback() {}
inproc void KrAudioPauseFallback() {}
inproc void KrAudioResetFallback() {}
inproc bool KrAudioSetRenderDeviceFallback(KrAudioDeviceId id) { return false; }
inproc uint KrAudioGetDevicesFallback(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) { return 0; }

inproc void KrEventFallback(const KrEvent *event, void *user) {}
inproc void KrUpdateFallback(void *user) {}
inproc u32  KrUploadAudioFallback(const KrAudioSpec *spec, u8 *data, u32 count, void *user) { return 0; }

const KrLibrary LibraryFallback = {
	.Audio = {
		.IsPlaying       = KrAudioIsPlayingFallback,
		.Update          = KrAudioUpdateFallback,
		.Resume          = KrAudioResumeFallback,
		.Pause           = KrAudioPauseFallback,
		.Reset           = KrAudioResetFallback,
		.SetRenderDevice = KrAudioSetRenderDeviceFallback,
		.GetDevices      = KrAudioGetDevicesFallback,
	},
};

KrLibrary g_Library = {
	.Audio = {
		.IsPlaying       = KrAudioIsPlayingFallback,
		.Update          = KrAudioUpdateFallback,
		.Resume          = KrAudioResumeFallback,
		.Pause           = KrAudioPauseFallback,
		.Reset           = KrAudioResetFallback,
		.SetRenderDevice = KrAudioSetRenderDeviceFallback,
		.GetDevices      = KrAudioGetDevicesFallback,
	},
};

KrUserContext g_UserContext = {
	.Data          = nullptr,
	.OnEvent       = KrEventFallback,
	.OnUpdate      = KrUpdateFallback,
	.OnUploadAudio = KrUploadAudioFallback,
};

proc bool KrAudioIsPlaying() {
	return g_Library.Audio.IsPlaying();
}

proc void KrAudioUpdate() {
	g_Library.Audio.Update();
}

proc void KrAudioResume() {
	g_Library.Audio.Resume();
}

proc void KrAudioPause() {
	g_Library.Audio.Pause();
}

proc void KrAudioReset() {
	g_Library.Audio.Reset();
}

proc bool KrAudioSetRenderDevice(KrAudioDeviceId id) {
	return g_Library.Audio.SetRenderDevice(id);
}

proc uint KrAudioGetDevices(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) {
	return g_Library.Audio.GetDevices(flow, inactive, output, cap);
}

const char *KrEventNamed(KrEventKind kind) {
	static const char *EventNames[] = {
		[KrEventKind_Startup]           = "Startup",
		[KrEventKind_Quit]              = "Quit",
		[KrEventKind_WindowCreated]     = "WindowCreated",
		[KrEventKind_WindowDestroyed]   = "WindowClosed",
		[KrEventKind_WindowActivated]   = "WindowActivated",
		[KrEventKind_WindowDeactivated] = "WindowDeactivated",
		[KrEventKind_WindowResized]     = "WindowResized",
		[KrEventKind_MouseMoved]        = "MouseMoved",
		[KrEventKind_ButtonPressed]     = "ButtonPressed",
		[KrEventKind_ButtonReleased]    = "ButtonReleased",
		[KrEventKind_DoubleClicked]     = "DoubleClicked",
		[KrEventKind_WheelMoved]        = "WheenMoved",
		[KrEventKind_KeyPressed]        = "KeyPressed",
		[KrEventKind_KeyReleased]       = "KeyReleased",
		[KrEventKind_TextInput]         = "TextInput",
	};
	static_assert(ArrayCount(EventNames) == KrEventKind_EnumMax, "");
	return EventNames[kind];
}
