#include "timestamp_input/timestamp_input.h"
#include "timestamp_input/timestamp_input_event.h"

#include <vector>

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;
using namespace GameInput::v3;

TimeStampInput *TimeStampInput::singleton_ = nullptr;

TimeStampInput::TimeStampInput() {
	singleton_ = this;
	key_press_counts_.fill(0);
	key_godot_codes_.fill(0);
}

TimeStampInput::~TimeStampInput() {
	stop();
	singleton_ = nullptr;
}

TimeStampInput *TimeStampInput::get_singleton() {
	return singleton_;
}

void TimeStampInput::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start"), &TimeStampInput::start);
	ClassDB::bind_method(D_METHOD("stop"), &TimeStampInput::stop);
	ClassDB::bind_method(D_METHOD("poll_events", "discard"), &TimeStampInput::poll_events, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_time_usec"), &TimeStampInput::get_time_usec);
}

bool TimeStampInput::start() {
	if (running_.load(std::memory_order_acquire)) {
		return true;
	}

	clear_device_states();
	event_queue_.reset();

	const HRESULT create_result = GameInputCreate(&game_input_);
	if (FAILED(create_result) || game_input_ == nullptr) {
		game_input_ = nullptr;
		UtilityFunctions::push_error(
			String("Failed to initialize Microsoft GameInput (HRESULT ") +
			String::num_int64(static_cast<int64_t>(create_result)) +
			")."
		);
		return false;
	}

	game_input_->SetFocusPolicy(GameInputDefaultFocusPolicy);
	sync_clock_offset();
	event_queue_.poll([this]() { return get_native_time_usec(); }, true);
	running_.store(true, std::memory_order_release);

	const HRESULT callback_result = game_input_->RegisterReadingCallback(
		nullptr,
		GameInputKindKeyboard,
		this,
		&TimeStampInput::reading_callback,
		&reading_callback_token_
	);

	if (FAILED(callback_result) || reading_callback_token_ == 0) {
		running_.store(false, std::memory_order_release);
		game_input_->Release();
		game_input_ = nullptr;
		reading_callback_token_ = 0;
		UtilityFunctions::push_error(
			String("Failed to register the GameInput keyboard callback (HRESULT ") +
			String::num_int64(static_cast<int64_t>(callback_result)) +
			")."
		);
		return false;
	}

	return true;
}

void TimeStampInput::stop() {
	if (!running_.exchange(false, std::memory_order_acq_rel)) {
		return;
	}

	if (game_input_ != nullptr && reading_callback_token_ != 0) {
		game_input_->StopCallback(reading_callback_token_);
		game_input_->UnregisterCallback(reading_callback_token_);
		reading_callback_token_ = 0;
	}

	event_queue_.reset();
	clear_device_states();

	if (game_input_ != nullptr) {
		game_input_->Release();
		game_input_ = nullptr;
	}
}

Dictionary TimeStampInput::poll_events(bool p_discard) {
	const auto batch = event_queue_.poll([this]() { return get_native_time_usec(); }, p_discard);
	TypedArray<TimeStampInputEvent> events;
	for (const auto &event : batch.events) {
		Ref<TimeStampInputEvent> event_obj;
		event_obj.instantiate();
		event_obj->set_native_keycode(event.keycode);
		event_obj->set_keycode(event.godot_keycode);
		event_obj->set_pressed(event.pressed);
		event_obj->set_timestamp_usec(align_to_godot_usec(event.timestamp_usec));
		event_obj->set_native_timestamp_usec(event.native_timestamp_usec);
		events.push_back(event_obj);
	}
	Dictionary result;
	result["events"] = events;
	result["cutoff_timestamp_usec"] = game_input_ != nullptr
		? align_to_godot_usec(batch.cutoff_timestamp_usec) : batch.cutoff_timestamp_usec;
	result["corrected_events"] = batch.corrected_events;
	return result;
}

uint64_t TimeStampInput::get_time_usec() const {
	return game_input_ != nullptr ? align_to_godot_usec(get_native_time_usec()) : get_native_time_usec();
}

void CALLBACK TimeStampInput::reading_callback(
	GameInputCallbackToken p_callback_token,
	void *p_context,
	IGameInputReading *p_reading
) {
	(void)p_callback_token;

	auto *instance = static_cast<TimeStampInput *>(p_context);
	if (instance == nullptr || p_reading == nullptr ||
		!instance->running_.load(std::memory_order_acquire)) {
		return;
	}

	instance->process_reading(p_reading);
}

void TimeStampInput::process_reading(IGameInputReading *p_reading) {
	IGameInputDevice *device = nullptr;
	p_reading->GetDevice(&device);
	if (device == nullptr) {
		return;
	}

	auto state_it = device_states_.find(device);
	if (state_it == device_states_.end()) {
		state_it = device_states_.try_emplace(device).first;
	} else {
		device->Release();
	}

	DeviceState &device_state = state_it->second;
	std::array<bool, 256> current_key_down {};
	std::array<uint32_t, 256> current_godot_codes {};

	const uint32_t key_count = p_reading->GetKeyCount();
	if (key_count > 0) {
		std::vector<GameInputKeyState> keys(key_count);
		const uint32_t returned_count = p_reading->GetKeyState(key_count, keys.data());

		for (uint32_t index = 0; index < returned_count; ++index) {
			const GameInputKeyState &key = keys[index];
			const uint32_t keycode = normalize_vkey(key);
			if (keycode == 0 || keycode >= current_key_down.size()) {
				continue;
			}

			current_key_down[keycode] = true;
			current_godot_codes[keycode] = vkey_to_godot_key(key.scanCode, keycode);
		}
	}

	const uint64_t timestamp_usec = p_reading->GetTimestamp();
	std::vector<TimeStampEventQueue::Event> events;
	for (uint32_t keycode = 0; keycode < current_key_down.size(); ++keycode) {
		const bool was_down = device_state.key_down[keycode];
		const bool is_down = current_key_down[keycode];
		if (was_down == is_down) {
			continue;
		}

		device_state.key_down[keycode] = is_down;
		if (is_down) {
			const uint16_t previous_count = key_press_counts_[keycode]++;
			if (previous_count == 0) {
				const uint32_t godot_keycode = current_godot_codes[keycode];
				key_godot_codes_[keycode] = godot_keycode;
				events.push_back({keycode, godot_keycode, true});
			}
		} else {
			if (key_press_counts_[keycode] > 0) {
				--key_press_counts_[keycode];
			}

			if (key_press_counts_[keycode] == 0) {
				const uint32_t godot_keycode = key_godot_codes_[keycode] != 0
					? key_godot_codes_[keycode]
					: vkey_to_godot_key(0, keycode);
				events.push_back({keycode, godot_keycode, false});
				key_godot_codes_[keycode] = 0;
			}
		}
	}
	event_queue_.push_reading(timestamp_usec, events);
}

void TimeStampInput::clear_device_states() {
	for (const auto &entry : device_states_) {
		entry.first->Release();
	}

	device_states_.clear();
	key_press_counts_.fill(0);
	key_godot_codes_.fill(0);
}

uint32_t TimeStampInput::normalize_vkey(const GameInputKeyState &p_key) const {
	const uint32_t virtual_key = p_key.virtualKey;
	if (virtual_key != VK_SHIFT && virtual_key != VK_CONTROL && virtual_key != VK_MENU) {
		return virtual_key;
	}

	const UINT mapped = MapVirtualKeyW(p_key.scanCode, MAPVK_VSC_TO_VK_EX);
	return mapped != 0 ? static_cast<uint32_t>(mapped) : virtual_key;
}

uint32_t TimeStampInput::vkey_to_godot_key(uint32_t p_scan_code, uint32_t p_virtual_key) const {
	if (p_virtual_key >= '0' && p_virtual_key <= '9') {
		return p_virtual_key;
	}
	if (p_virtual_key >= 'A' && p_virtual_key <= 'Z') {
		return p_virtual_key;
	}
	if (p_virtual_key >= VK_F1 && p_virtual_key <= VK_F24) {
		return KEY_F1 + (p_virtual_key - VK_F1);
	}
	if (p_virtual_key >= VK_NUMPAD0 && p_virtual_key <= VK_NUMPAD9) {
		return KEY_KP_0 + (p_virtual_key - VK_NUMPAD0);
	}

	switch (p_virtual_key) {
		case VK_ESCAPE:
			return KEY_ESCAPE;
		case VK_TAB:
			return KEY_TAB;
		case VK_BACK:
			return KEY_BACKSPACE;
		case VK_RETURN:
			return (p_scan_code & 0xE000U) != 0 ? KEY_KP_ENTER : KEY_ENTER;
		case VK_SPACE:
			return KEY_SPACE;
		case VK_INSERT:
			return KEY_INSERT;
		case VK_DELETE:
			return KEY_DELETE;
		case VK_PAUSE:
			return KEY_PAUSE;
		case VK_SNAPSHOT:
			return KEY_PRINT;
		case VK_CLEAR:
			return KEY_CLEAR;
		case VK_HOME:
			return KEY_HOME;
		case VK_END:
			return KEY_END;
		case VK_LEFT:
			return KEY_LEFT;
		case VK_UP:
			return KEY_UP;
		case VK_RIGHT:
			return KEY_RIGHT;
		case VK_DOWN:
			return KEY_DOWN;
		case VK_PRIOR:
			return KEY_PAGEUP;
		case VK_NEXT:
			return KEY_PAGEDOWN;
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			return KEY_SHIFT;
		case VK_CONTROL:
		case VK_LCONTROL:
		case VK_RCONTROL:
			return KEY_CTRL;
		case VK_MENU:
		case VK_LMENU:
		case VK_RMENU:
			return KEY_ALT;
		case VK_LWIN:
		case VK_RWIN:
			return KEY_META;
		case VK_CAPITAL:
			return KEY_CAPSLOCK;
		case VK_NUMLOCK:
			return KEY_NUMLOCK;
		case VK_SCROLL:
			return KEY_SCROLLLOCK;
		case VK_APPS:
			return KEY_MENU;
		case VK_MULTIPLY:
			return KEY_KP_MULTIPLY;
		case VK_DIVIDE:
			return KEY_KP_DIVIDE;
		case VK_SUBTRACT:
			return KEY_KP_SUBTRACT;
		case VK_DECIMAL:
			return KEY_KP_PERIOD;
		case VK_ADD:
			return KEY_KP_ADD;
		case VK_BROWSER_BACK:
			return KEY_BACK;
		case VK_BROWSER_FORWARD:
			return KEY_FORWARD;
		case VK_BROWSER_REFRESH:
			return KEY_REFRESH;
		case VK_BROWSER_STOP:
			return KEY_STOP;
		case VK_BROWSER_SEARCH:
			return KEY_SEARCH;
		case VK_BROWSER_FAVORITES:
			return KEY_FAVORITES;
		case VK_BROWSER_HOME:
			return KEY_HOMEPAGE;
		case VK_VOLUME_MUTE:
			return KEY_VOLUMEMUTE;
		case VK_VOLUME_DOWN:
			return KEY_VOLUMEDOWN;
		case VK_VOLUME_UP:
			return KEY_VOLUMEUP;
		case VK_MEDIA_NEXT_TRACK:
			return KEY_MEDIANEXT;
		case VK_MEDIA_PREV_TRACK:
			return KEY_MEDIAPREVIOUS;
		case VK_MEDIA_STOP:
			return KEY_MEDIASTOP;
		case VK_MEDIA_PLAY_PAUSE:
			return KEY_MEDIAPLAY;
		case VK_LAUNCH_MAIL:
			return KEY_LAUNCHMAIL;
		case VK_LAUNCH_MEDIA_SELECT:
			return KEY_LAUNCHMEDIA;
		case VK_LAUNCH_APP1:
			return KEY_LAUNCH0;
		case VK_LAUNCH_APP2:
			return KEY_LAUNCH1;
		case VK_SLEEP:
			return KEY_STANDBY;
		case VK_HELP:
			return KEY_HELP;
		case VK_OEM_1:
			return KEY_SEMICOLON;
		case VK_OEM_PLUS:
			return KEY_EQUAL;
		case VK_OEM_COMMA:
			return KEY_COMMA;
		case VK_OEM_MINUS:
			return KEY_MINUS;
		case VK_OEM_PERIOD:
			return KEY_PERIOD;
		case VK_OEM_2:
			return KEY_SLASH;
		case VK_OEM_3:
			return KEY_QUOTELEFT;
		case VK_OEM_4:
			return KEY_BRACKETLEFT;
		case VK_OEM_5:
			return KEY_BACKSLASH;
		case VK_OEM_6:
			return KEY_BRACKETRIGHT;
		case VK_OEM_7:
			return KEY_APOSTROPHE;
		default:
			return KEY_UNKNOWN;
	}
}

void TimeStampInput::sync_clock_offset() {
	Time *time_singleton = Time::get_singleton();
	if (time_singleton == nullptr || game_input_ == nullptr) {
		godot_clock_offset_usec_.store(0, std::memory_order_release);
		return;
	}

	const int64_t godot_usec = time_singleton->get_ticks_usec();
	const int64_t native_usec = static_cast<int64_t>(game_input_->GetCurrentTimestamp());
	godot_clock_offset_usec_.store(godot_usec - native_usec, std::memory_order_release);
}

uint64_t TimeStampInput::get_native_time_usec() const {
	if (game_input_ != nullptr) {
		return game_input_->GetCurrentTimestamp();
	}

	Time *time_singleton = Time::get_singleton();
	return time_singleton != nullptr ? time_singleton->get_ticks_usec() : 0;
}

uint64_t TimeStampInput::align_to_godot_usec(uint64_t p_native_usec) const {
	const int64_t offset_usec = godot_clock_offset_usec_.load(std::memory_order_acquire);
	const int64_t native_usec = static_cast<int64_t>(p_native_usec);
	const int64_t aligned_usec = native_usec + offset_usec;

	return aligned_usec > 0 ? static_cast<uint64_t>(aligned_usec) : 0;
}
