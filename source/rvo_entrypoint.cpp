namespace rvo { void entrypoint(); }

#include "rvo_rdoc.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

int main(int argc, char const* argv[]) {
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