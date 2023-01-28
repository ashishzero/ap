#include "KrMediaImpl.h"

inproc bool KrAudioIsPlayingFallback() { return false; }
inproc void KrAudioUpdateFallback() {}
inproc void KrAudioResumeFallback() {}
inproc void KrAudioPauseFallback() {}
inproc void KrAudioResetFallback() {}
inproc bool KrAudioSetRenderDeviceFallback(KrAudioDeviceId id) { return false; }
inproc uint KrAudioGetDevicesFallback(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) { return 0; }

inproc u32  KrUploadAudioFallback(const KrAudioSpec *spec, u8 *data, u32 count, void *user) { return 0; }

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

KrUserConfig g_UserConfig = {
	.Data           = nullptr,
	.OnUploadAudio = KrUploadAudioFallback
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

KrUserConfig *KrUserConfigGet() {
	return &g_UserConfig;
}

proc int KrRun() {
	return g_Library.Main.Run();
}
