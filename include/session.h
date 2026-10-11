#pragma once
#include <stdint.h>
#include <string>

// Portable state machine; elapsed subtraction remains valid across millis rollover.
class RentalSession {
 public:
  enum Result { OK, CONFLICT, INVALID };
  static constexpr uint32_t MAX_SECONDS = 86400;
  bool active = false;
  std::string id;
  uint32_t revision = 0;
  uint32_t generation = 0;

  void forceOff() {
    if (active) ++generation;
    active = false;
  }
  void tick(uint32_t now) {
    if (active && uint32_t(now - since_) >= durationMs_) active = false;
  }
  uint32_t remaining(uint32_t now) const {
    if (!active) return 0;
    uint32_t elapsed = now - since_;
    return elapsed >= durationMs_ ? 0 : (durationMs_ - elapsed + 999) / 1000;
  }
  Result apply(const std::string &op, const std::string &newId,
               uint32_t rev, uint32_t seconds, uint32_t now, uint32_t expectedGeneration = 0) {
    tick(now);
    if (newId.empty() || newId.size() > 64 || rev == 0 ||
        (op != "start" && op != "update" && op != "stop") ||
        (op != "stop" && (seconds == 0 || seconds > MAX_SECONDS))) return INVALID;
    if (newId == id && rev == revision) {
      return op == lastOp_ && seconds == lastSeconds_ ? OK : CONFLICT;
    }
    if (op == "start") {
      if (active || newId == id || rev != 1 || expectedGeneration != generation) return CONFLICT;
      ++generation;
    } else {
      if (newId != id || rev <= revision || (op == "update" && !active)) return CONFLICT;
    }
    id = newId;
    revision = rev;
    lastOp_ = op;
    lastSeconds_ = seconds;
    active = op != "stop";
    since_ = now;
    durationMs_ = seconds * 1000;
    return OK;
  }
 private:
  uint32_t since_ = 0, durationMs_ = 0, lastSeconds_ = 0;
  std::string lastOp_;
};
