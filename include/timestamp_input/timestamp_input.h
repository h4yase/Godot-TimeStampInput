#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <GameInput.h>
#include "timestamp_event_queue.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <unordered_map>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

class TimeStampInputEvent;

class TimeStampInput final : public godot::Object {
	GDCLASS(TimeStampInput, godot::Object)

public:
	TimeStampInput();
	~TimeStampInput() override;

	static TimeStampInput *get_singleton();

	bool start();
	void stop();
	godot::Dictionary poll_events(bool p_discard = false);
	uint64_t get_time_usec() const;

protected:
	static void _bind_methods();

private:
	struct DeviceState {
		std::array<bool, 256> key_down {};
	};

	static void CALLBACK reading_callback(
		GameInput::v3::GameInputCallbackToken p_callback_token,
		void *p_context,
		GameInput::v3::IGameInputReading *p_reading
	);

	void process_reading(GameInput::v3::IGameInputReading *p_reading);
	void clear_device_states();
	uint32_t normalize_vkey(const GameInput::v3::GameInputKeyState &p_key) const;
	uint32_t vkey_to_godot_key(uint32_t p_scan_code, uint32_t p_virtual_key) const;
	void sync_clock_offset();
	uint64_t get_native_time_usec() const;
	uint64_t align_to_godot_usec(uint64_t p_native_usec) const;

	static TimeStampInput *singleton_;

	TimeStampEventQueue event_queue_;
	std::atomic<bool> running_ {false};
	std::atomic<int64_t> godot_clock_offset_usec_ {0};

	GameInput::v3::IGameInput *game_input_ = nullptr;
	GameInput::v3::GameInputCallbackToken reading_callback_token_ = 0;
	std::unordered_map<GameInput::v3::IGameInputDevice *, DeviceState> device_states_;
	std::array<uint16_t, 256> key_press_counts_ {};
	std::array<uint32_t, 256> key_godot_codes_ {};
};
