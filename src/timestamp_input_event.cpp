#include "timestamp_input/timestamp_input_event.h"

using namespace godot;

void TimeStampInputEvent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_native_keycode", "value"), &TimeStampInputEvent::set_native_keycode);
	ClassDB::bind_method(D_METHOD("get_native_keycode"), &TimeStampInputEvent::get_native_keycode);
	ClassDB::bind_method(D_METHOD("set_keycode", "value"), &TimeStampInputEvent::set_keycode);
	ClassDB::bind_method(D_METHOD("get_keycode"), &TimeStampInputEvent::get_keycode);
	ClassDB::bind_method(D_METHOD("set_pressed", "value"), &TimeStampInputEvent::set_pressed);
	ClassDB::bind_method(D_METHOD("is_pressed"), &TimeStampInputEvent::is_pressed);
	ClassDB::bind_method(D_METHOD("set_timestamp_usec", "value"), &TimeStampInputEvent::set_timestamp_usec);
	ClassDB::bind_method(D_METHOD("get_timestamp_usec"), &TimeStampInputEvent::get_timestamp_usec);
	ClassDB::bind_method(D_METHOD("set_native_timestamp_usec", "value"), &TimeStampInputEvent::set_native_timestamp_usec);
	ClassDB::bind_method(D_METHOD("get_native_timestamp_usec"), &TimeStampInputEvent::get_native_timestamp_usec);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "native_keycode"), "set_native_keycode", "get_native_keycode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "keycode"), "set_keycode", "get_keycode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pressed"), "set_pressed", "is_pressed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "timestamp_usec"), "set_timestamp_usec", "get_timestamp_usec");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "native_timestamp_usec"), "set_native_timestamp_usec", "get_native_timestamp_usec");
}

void TimeStampInputEvent::set_native_keycode(uint32_t p_value) {
	native_keycode_ = p_value;
}

uint32_t TimeStampInputEvent::get_native_keycode() const {
	return native_keycode_;
}

void TimeStampInputEvent::set_keycode(uint32_t p_value) {
	keycode_ = p_value;
}

uint32_t TimeStampInputEvent::get_keycode() const {
	return keycode_;
}

void TimeStampInputEvent::set_pressed(bool p_value) {
	pressed_ = p_value;
}

bool TimeStampInputEvent::is_pressed() const {
	return pressed_;
}

void TimeStampInputEvent::set_timestamp_usec(uint64_t p_value) {
	timestamp_usec_ = p_value;
}

uint64_t TimeStampInputEvent::get_timestamp_usec() const {
	return timestamp_usec_;
}

void TimeStampInputEvent::set_native_timestamp_usec(uint64_t p_value) {
	native_timestamp_usec_ = p_value;
}

uint64_t TimeStampInputEvent::get_native_timestamp_usec() const {
	return native_timestamp_usec_;
}
