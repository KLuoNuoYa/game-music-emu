#ifndef GME_INNO_H
#define GME_INNO_H

#ifdef _WIN32
    #ifdef GME_INNO_BUILD_DLL
        #define GME_INNO_EXPORT __declspec(dllexport)
    #else
        #define GME_INNO_EXPORT __declspec(dllimport)
    #endif
    #define GME_INNO_CALL __stdcall
#else
    #define GME_INNO_EXPORT
    #define GME_INNO_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 on success, 0 on failure. Paths are UTF-16. */
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoOpenFileW(const wchar_t* path, int sample_rate);
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoOpenFileTrackW(const wchar_t* path, int sample_rate, int track_index);
GME_INNO_EXPORT void GME_INNO_CALL GMEInnoClose(void);

/* Playback control */
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoStartTrack(int track_index);
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoPlay(void);
GME_INNO_EXPORT void GME_INNO_CALL GMEInnoPause(void);
GME_INNO_EXPORT void GME_INNO_CALL GMEInnoStop(void);
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoIsPlaying(void);

/* Configuration */
GME_INNO_EXPORT void GME_INNO_CALL GMEInnoSetLoop(int enabled);
GME_INNO_EXPORT void GME_INNO_CALL GMEInnoSetVolume(int volume_percent);

/* Informational */
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoGetTrackCount(void);
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoGetCurrentTrack(void);
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoGetLastErrorW(wchar_t* buffer, int capacity);
GME_INNO_EXPORT int GME_INNO_CALL GMEInnoGetLastErrorLength(void);

#ifdef __cplusplus
}
#endif

#endif
