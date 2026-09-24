#include "register_types.h"

#include "timestamp_input/timestamp_input_event.h"
#include "timestamp_input/timestamp_input.h"

#include <godot_cpp/classes/engine.hpp>

using namespace godot;

static TimeStampInput *timestamp_input_singleton = nullptr;

void initialize_timestamp_input_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<TimeStampInput>();
	ClassDB::register_class<TimeStampInputEvent>();

	timestamp_input_singleton = memnew(TimeStampInput);
	Engine::get_singleton()->register_singleton("TimeStampInput", timestamp_input_singleton);
}

void uninitialize_timestamp_input_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (timestamp_input_singleton != nullptr) {
		Engine::get_singleton()->unregister_singleton("TimeStampInput");
		memdelete(timestamp_input_singleton);
		timestamp_input_singleton = nullptr;
	}
}
