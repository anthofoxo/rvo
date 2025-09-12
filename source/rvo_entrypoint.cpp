namespace rvo { void entrypoint(); }

#include "rvo_rdoc.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

int main(int argc, char* argv[]) {
	using namespace std::chrono_literals;
	spdlog::set_level(spdlog::level::trace);
	spdlog::default_logger()->sinks().push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true));
	spdlog::flush_every(5s);
	spdlog::flush_on(spdlog::level::err);
	spdlog::flush_on(spdlog::level::critical);
	rvo::rdoc::setup(true);
	rvo::entrypoint();
	spdlog::shutdown();
}

#ifdef RVO_USE_WINMAIN
#	include <Windows.h>
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
	AllocConsole();

	return main(__argc, __argv);
}
#endif