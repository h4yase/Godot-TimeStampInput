#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <vector>

class TimeStampEventQueue {
public:
	struct Event {
		uint32_t keycode = 0;
		uint32_t godot_keycode = 0;
		bool pressed = false;
		uint64_t timestamp_usec = 0;
		uint64_t native_timestamp_usec = 0;
	};

	struct Batch {
		std::vector<Event> events;
		uint64_t cutoff_timestamp_usec = 0;
		uint64_t corrected_events = 0;
	};

	void reset() {
		std::lock_guard<std::mutex> lock(mutex_);
		pending_.clear();
		cutoff_ = 0;
		discard_before_ = 0;
		last_event_time_ = 0;
		corrected_events_ = 0;
	}

	void push_reading(uint64_t p_timestamp, const std::vector<Event> &p_events) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (p_timestamp <= discard_before_) {
			return;
		}
		// GameInput may dispatch an old reading after a poll. Correction and publication
		// must share the poll lock; even an equal usec belongs after the closed cutoff.
		const uint64_t effective_time = std::max({p_timestamp, cutoff_ + 1, last_event_time_});
		for (Event event : p_events) {
			event.native_timestamp_usec = p_timestamp;
			event.timestamp_usec = effective_time;
			pending_.push_back(event);
			corrected_events_ += effective_time != p_timestamp;
		}
		if (!p_events.empty()) {
			last_event_time_ = effective_time;
		}
	}

	template <typename Clock>
	Batch poll(Clock p_now, bool p_discard = false) {
		std::lock_guard<std::mutex> lock(mutex_);
		Batch batch;
		const uint64_t now = p_now();
		assert(now >= cutoff_);
		cutoff_ = now;
		batch.cutoff_timestamp_usec = cutoff_;
		batch.corrected_events = corrected_events_;
		corrected_events_ = 0;
		if (p_discard) {
			pending_.clear();
			discard_before_ = cutoff_;
		} else {
			// A correction to cutoff+1 may be ahead of a second poll in the same usec.
			const auto end = std::upper_bound(pending_.begin(), pending_.end(), cutoff_,
				[](uint64_t time, const Event &event) { return time < event.timestamp_usec; });
			batch.events.assign(pending_.begin(), end);
			pending_.erase(pending_.begin(), end);
		}
		return batch;
	}

private:
	std::mutex mutex_;
	std::vector<Event> pending_;
	uint64_t cutoff_ = 0;
	uint64_t discard_before_ = 0;
	uint64_t last_event_time_ = 0;
	uint64_t corrected_events_ = 0;
};
