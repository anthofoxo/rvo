#include "rvo_rdoc.hpp"

#include "renderdoc_app.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#	include <Windows.h>
#endif

#ifdef __linux__
#	include <dlfcn.h>
#endif

namespace rvo::rdoc {
	namespace {
		RENDERDOC_API_1_0_0* gApi = nullptr;
	}

	bool attached() {
		return gApi;
	}

	void trigger_capture() {
		if (gApi) gApi->TriggerCapture();
	}

	void launch_replay_ui() {
		if (gApi) gApi->LaunchReplayUI(1, nullptr);
	}

	void setup() {
#ifdef _WIN32
		// Attach the renderdoc dll at startup if it isn't already
		// This operation only works on windows, for unix systems renderdoc itself MUST spawn the application
		if (GetModuleHandleA("renderdoc.dll") == nullptr) {
			if (LoadLibraryA("C:/Program Files/RenderDoc/renderdoc.dll")) {
				spdlog::info("Successfully attached renderdoc");
			}
		}
#endif
#ifdef _WIN32
		if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
			int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_0_0, (void**)&gApi);
			assert(ret == 1);
		}
#elif defined(__linux__)
		if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
			int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_0_0, (void**)&gApi);
			assert(ret == 1);
		}
#else
		spdlog::warn("Cannot detect RenderDoc: Unsupported platform");
#endif

		if (!gApi) return;

		int major, minor, patch;
		gApi->GetAPIVersion(&major, &minor, &patch);
		spdlog::debug("Requested v{}.{}.{}, got v{}.{}.{}", 1, 0, 0, major, minor, patch);

		spdlog::info("RenderDoc Detected, Applying RVO RenderDoc Preferences");

		auto captureKey = eRENDERDOC_Key_F8;
		gApi->MaskOverlayBits(0, 0);
		gApi->SetCaptureKeys(&captureKey, 1);
		gApi->SetCaptureOptionU32(eRENDERDOC_Option_DebugOutputMute, 0);
		gApi->SetCaptureOptionU32(eRENDERDOC_Option_APIValidation, 1);
		gApi->SetCaptureOptionU32(eRENDERDOC_Option_RefAllResources, 1);
	}
}