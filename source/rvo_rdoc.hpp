#pragma once

namespace rvo::rdoc {
	void setup(bool aAttachOnLaunch);
	bool attached();
	void trigger_capture();
	void launch_replay_ui();
}