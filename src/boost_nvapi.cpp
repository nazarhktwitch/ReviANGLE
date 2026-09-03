// Boost: NVAPI driver profile  (real DRS implementation)
// ------------------------------------------------------------------------
// Sets per-application Nvidia driver settings via the DRS (Display Driver
// Settings) API. These persist in the user's Nvidia profile DB and survive
// across launches:
//
//   PREFERRED_PSTATE         = P0       - pin GPU to max performance state
//                                          (no clock-spike jitter; GT 630M
//                                          otherwise idles to 270 MHz and
//                                          ramps slowly under load)
//   POWER_MIZER_LEVEL_AC     = MAX_PERF - driver power policy: max perf
//   THREADED_OPTIMIZATION    = ON       - driver-side multi-threaded
//                                          submission (helps on >2-core CPU,
//                                          neutral on 2-core)
//   VSYNC_MODE               = OFF      - driver-level vsync forced off
//                                          (combines with our allow_tearing
//                                          to guarantee no driver-side wait)
//   FRAME_LIMITER            = OFF      - kill driver-imposed FPS cap
//
// Silent no-op on AMD/Intel GPUs (DLL load fails).
//
// NVAPI is undocumented; IDs and setting hashes come from public reverse-
// engineering (NVidiaProfileInspector / nvapi-open-source-sdk-headers).

#include "angle_loader.hpp"
#include "config.hpp"
#include <windows.h>


// ===== NVAPI minimal type subset (avoid nvapi.h dependency) =====
typedef int NvAPI_Status;
typedef unsigned int NvU32;
typedef void *NvDRSSessionHandle;
typedef void *NvDRSProfileHandle;
typedef wchar_t NvAPI_UnicodeString[2048];
#define NVAPI_OK 0
#define NVAPI_SETTING_MAX_VALUES 100

enum NVDRS_SETTING_TYPE {
  NVDRS_DWORD_TYPE = 0,
  NVDRS_BINARY_TYPE = 1,
  NVDRS_STRING_TYPE = 2,
  NVDRS_WSTRING_TYPE = 3
};

typedef struct {
  NvU32 version;
  NvAPI_UnicodeString settingName;
  NvU32 settingId;
  NVDRS_SETTING_TYPE settingType;
  NvU32 settingLocation;
  NvU32 isCurrentPredefined;
  NvU32 isPredefinedValid;
  NvU32 u32PredefinedValue;
  NvU32 u32CurrentValue;
} NVDRS_SETTING_V1;

#define NVDRS_SETTING_VER ((NvU32)(sizeof(NVDRS_SETTING_V1) | (1 << 16)))

typedef NvAPI_Status(__cdecl *NvAPI_Initialize_t)();
typedef void *(__cdecl *NvAPI_QueryInterface_t)(NvU32);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_CreateSession_t)(NvDRSSessionHandle *);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_DestroySession_t)(NvDRSSessionHandle);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_LoadSettings_t)(NvDRSSessionHandle);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_SaveSettings_t)(NvDRSSessionHandle);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_GetCurrentGlobalProfile_t)(
    NvDRSSessionHandle, NvDRSProfileHandle *);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_FindApplicationByName_t)(
    NvDRSSessionHandle, const wchar_t *, NvDRSProfileHandle *, void *);
typedef NvAPI_Status(__cdecl *NvAPI_DRS_SetSetting_t)(NvDRSSessionHandle,
                                                      NvDRSProfileHandle,
                                                      NVDRS_SETTING_V1 *);

// Function IDs reverse-engineered from public NVAPI headers.
// Stable across Nvidia driver versions since 2010.
#define NVID_INITIALIZE 0x0150E828
#define NVID_DRS_CREATE_SESSION 0x0694D52E
#define NVID_DRS_DESTROY_SESSION 0xDAD9CFF8
#define NVID_DRS_LOAD_SETTINGS 0x375DBD6B
#define NVID_DRS_SAVE_SETTINGS 0xFCBC7E14
#define NVID_DRS_GET_CURRENT_GLOBAL_PROF 0x617BFF9F
#define NVID_DRS_FIND_APP_BY_NAME 0xEEE566B2
#define NVID_DRS_SET_SETTING 0x577DD202

// DRS setting IDs (from nvapi headers).
#define PREFERRED_PSTATE_ID 0x1057EB71
#define PREFERRED_PSTATE_PREFER_MAX 0
#define POWER_MIZER_LEVEL_AC_ID 0x10D690E4
#define POWER_MIZER_PREFER_MAX_PERF 0x1
#define VSYNC_MODE_ID 0x00A879CF
#define VSYNC_MODE_FORCE_OFF 0x33E92A2A
#define OGL_THREAD_CONTROL_ID 0x20C1221E
#define OGL_THREAD_CONTROL_ENABLE 0x00000001
#define FRAME_LIMITER_ID 0x10835986
#define FRAME_LIMITER_OFF 0

namespace boost_nvapi {

static bool setOne(NvAPI_DRS_SetSetting_t setSetting, NvDRSSessionHandle ses,
                   NvDRSProfileHandle prof, NvU32 id, NvU32 value,
                   const char *nameForLog) {
  NVDRS_SETTING_V1 s = {};
  s.version = NVDRS_SETTING_VER;
  s.settingId = id;
  s.settingType = NVDRS_DWORD_TYPE;
  s.u32CurrentValue = value;
  s.u32PredefinedValue = value;
  NvAPI_Status r = setSetting(ses, prof, &s);
  if (r == NVAPI_OK) {
    angle::log("nvapi: %s = 0x%08X applied", nameForLog, value);
    return true;
  }
  angle::log("nvapi: %s SetSetting failed (%d)", nameForLog, r);
  return false;
}

void apply() {
  if (!Config::get().nvapi_profile)
    return;

  HMODULE nv = LoadLibraryA("nvapi64.dll");
  if (!nv)
    nv = LoadLibraryA("nvapi.dll");
  if (!nv) {
    angle::log("nvapi: not found (non-Nvidia system), skipping");
    return;
  }

  auto qi = (NvAPI_QueryInterface_t)GetProcAddress(nv, "nvapi_QueryInterface");
  if (!qi) {
    angle::log("nvapi: queryInterface missing");
    FreeLibrary(nv);
    return;
  }

  // Resolve all entry points via QI.
  auto fnInit = (NvAPI_Initialize_t)qi(NVID_INITIALIZE);
  auto fnCreate = (NvAPI_DRS_CreateSession_t)qi(NVID_DRS_CREATE_SESSION);
  auto fnDestroy = (NvAPI_DRS_DestroySession_t)qi(NVID_DRS_DESTROY_SESSION);
  auto fnLoad = (NvAPI_DRS_LoadSettings_t)qi(NVID_DRS_LOAD_SETTINGS);
  auto fnSave = (NvAPI_DRS_SaveSettings_t)qi(NVID_DRS_SAVE_SETTINGS);
  auto fnGetGlobal =
      (NvAPI_DRS_GetCurrentGlobalProfile_t)qi(NVID_DRS_GET_CURRENT_GLOBAL_PROF);
  auto fnSet = (NvAPI_DRS_SetSetting_t)qi(NVID_DRS_SET_SETTING);

  if (!fnInit || !fnCreate || !fnDestroy || !fnLoad || !fnSave ||
      !fnGetGlobal || !fnSet) {
    angle::log(
        "nvapi: some entry points missing - driver too old? skipping DRS");
    FreeLibrary(nv);
    return;
  }

  if (fnInit() != NVAPI_OK) {
    angle::log("nvapi: NvAPI_Initialize failed (no active Nvidia GPU)");
    FreeLibrary(nv);
    return;
  }

  NvDRSSessionHandle ses = nullptr;
  if (fnCreate(&ses) != NVAPI_OK || !ses) {
    angle::log("nvapi: DRS_CreateSession failed");
    FreeLibrary(nv);
    return;
  }
  if (fnLoad(ses) != NVAPI_OK) {
    angle::log("nvapi: DRS_LoadSettings failed");
    fnDestroy(ses);
    FreeLibrary(nv);
    return;
  }

  // Find the app's existing profile (if Nvidia ships one) - apply settings
  // there. Fallback to global profile so settings still take effect.
  NvDRSProfileHandle prof = nullptr;
  wchar_t exePath[MAX_PATH];
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  const wchar_t *exeName = wcsrchr(exePath, L'\\');
  exeName = exeName ? exeName + 1 : exePath;

  auto fnFindApp =
      (NvAPI_DRS_FindApplicationByName_t)qi(NVID_DRS_FIND_APP_BY_NAME);
  bool gotAppProfile = false;
  if (fnFindApp) {
    // FindApplicationByName takes (session, name, &profile, &app); we pass
    // a small scratch buffer for the app-info out-param.
    NvU32 appScratch[64] = {};
    if (fnFindApp(ses, exeName, &prof, appScratch) == NVAPI_OK && prof) {
      gotAppProfile = true;
      angle::log("nvapi: using app profile for %ls", exeName);
    }
  }
  if (!gotAppProfile) {
    if (fnGetGlobal(ses, &prof) != NVAPI_OK || !prof) {
      angle::log("nvapi: no profile available, aborting DRS");
      fnDestroy(ses);
      FreeLibrary(nv);
      return;
    }
    angle::log(
        "nvapi: applying to global base profile (no app profile for %ls)",
        exeName);
  }

  int applied = 0;
  applied += setOne(fnSet, ses, prof, PREFERRED_PSTATE_ID,
                    PREFERRED_PSTATE_PREFER_MAX, "PREFERRED_PSTATE=PreferMax");
  applied +=
      setOne(fnSet, ses, prof, POWER_MIZER_LEVEL_AC_ID,
             POWER_MIZER_PREFER_MAX_PERF, "POWER_MIZER_LEVEL_AC=PreferMaxPerf");
  applied += setOne(fnSet, ses, prof, VSYNC_MODE_ID, VSYNC_MODE_FORCE_OFF,
                    "VSYNC_MODE=ForceOff");
  applied +=
      setOne(fnSet, ses, prof, OGL_THREAD_CONTROL_ID, OGL_THREAD_CONTROL_ENABLE,
             "OGL_THREADED_OPTIMIZATION=Enable");
  applied += setOne(fnSet, ses, prof, FRAME_LIMITER_ID, FRAME_LIMITER_OFF,
                    "FRAME_LIMITER=Off");

  if (fnSave(ses) == NVAPI_OK) {
    angle::log("nvapi: DRS settings saved (%d/5 applied)", applied);
  } else {
    angle::log("nvapi: DRS_SaveSettings failed - runtime-only effect");
  }
  fnDestroy(ses);
  // Don't FreeLibrary - keep NVAPI loaded so driver profile stays active.
}
} // namespace boost_nvapi
