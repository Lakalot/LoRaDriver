#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace loradriver {

// Forward declarations for profile integration (Story 3.1)
struct HardwareProfile;
class ProfileQualificationMatrix;

enum class GateSeverity : uint8_t {
  kBlocking = 0,
  kWarning = 1,
  kAdvisory = 2
};

enum class GateCategory : uint8_t {
  kInit = 0,
  kTxRx = 1,
  kIrq = 2,
  kTimeout = 3,
  kRecovery = 4,
  kIntegration = 5
};

enum class ThresholdOperator : uint8_t {
  kGreaterOrEqual = 0,
  kLessOrEqual = 1,
  kEqual = 2,
  kLess = 3,
  kGreater = 4
};

struct GoNoGoThreshold {
  static constexpr size_t kMaxMetricNameLength = 32;
  static constexpr size_t kMaxUnitLength = 16;
  static constexpr float kEpsilon = 1e-6f;

  char metric_name[kMaxMetricNameLength];
  ThresholdOperator op;
  float value;
  char unit[kMaxUnitLength];

  [[nodiscard]] constexpr bool evaluate(float actual_value) const noexcept {
    switch (op) {
      case ThresholdOperator::kGreaterOrEqual:
        return actual_value >= value - kEpsilon;
      case ThresholdOperator::kLessOrEqual:
        return actual_value <= value + kEpsilon;
      case ThresholdOperator::kEqual:
        return (actual_value >= value - kEpsilon) && (actual_value <= value + kEpsilon);
      case ThresholdOperator::kLess:
        return actual_value < value - kEpsilon;
      case ThresholdOperator::kGreater:
        return actual_value > value + kEpsilon;
      default:
        return false;
    }
  }
};

struct GateRule {
  static constexpr size_t kMaxIdLength = 16;
  static constexpr size_t kMaxDescriptionLength = 128;

  char id[kMaxIdLength];
  GateCategory category;
  GateSeverity severity;
  GoNoGoThreshold threshold;
  char description[kMaxDescriptionLength];
  bool enabled;

  [[nodiscard]] constexpr bool isBlocking() const noexcept {
    return severity == GateSeverity::kBlocking && enabled;
  }
};

enum class GateResult : uint8_t {
  kPass = 0,
  kFail = 1,
  kWaived = 2,
  kDisabled = 3,
  kError = 4
};

struct GateEvaluation {
  char gate_id[GateRule::kMaxIdLength];
  GateResult result;
  float actual_value;
  float threshold_value;
  bool threshold_met;
};

struct GateWaiver {
  static constexpr size_t kMaxGateIdLength = GateRule::kMaxIdLength;
  static constexpr size_t kMaxJustificationLength = 256;
  static constexpr size_t kMaxApproverLength = 64;

  char gate_id[kMaxGateIdLength];
  char justification[kMaxJustificationLength];
  char approver[kMaxApproverLength];
  uint32_t waiver_id;
  uint32_t approved_by_id;
  uint8_t is_approved;
  uint8_t is_expired;
  uint32_t expiry_timestamp;
  uint32_t created_timestamp;
};

enum class ReleaseChannel : uint8_t {
  kRegular = 0,
  kHotfix = 1
};

struct ChannelPolicy {
  static constexpr size_t kMaxRequiredGates = 16;
  static constexpr size_t kMaxApprovers = 8;

  ReleaseChannel channel;
  std::array<char[GateRule::kMaxIdLength], kMaxRequiredGates> required_gates;
  uint8_t required_gates_count;
  bool allow_waivers;
  std::array<char[64], kMaxApprovers> waiver_approvers;
  uint8_t approvers_count;
};

struct GateReport {
  static constexpr size_t kMaxGateEvaluations = 32;

  std::array<GateEvaluation, kMaxGateEvaluations> evaluations;
  uint8_t evaluations_count;
  uint8_t blocking_passed;
  uint8_t blocking_failed;
  uint8_t warning_count;
  uint8_t advisory_count;
  bool release_blocked;
  uint8_t report_major;
  uint8_t report_minor;
  uint8_t report_patch;
};

class CiGateEngine {
 public:
  static constexpr size_t kMaxGateRules = 32;
  static constexpr size_t kMaxWaivers = 16;

  [[nodiscard]] static const GateRule* getGateRule(const char* gate_id) noexcept;
  [[nodiscard]] static GateResult evaluateGate(const char* gate_id, float actual_value) noexcept;
  [[nodiscard]] static GateResult evaluateGate(const GateRule& rule, float actual_value) noexcept;
  static void evaluateAllGates(const float* values, size_t values_count, GateReport& out_report) noexcept;
  [[nodiscard]] static bool isReleaseBlocked(const GateReport& report) noexcept;
  static void getFailedBlockingGates(const GateReport& report,
                                     std::array<GateEvaluation, 16>& out_failed,
                                     uint8_t& out_count) noexcept;

  /// Evaluate gates only for a validated profile (Story 3.1 integration).
  /// Returns kError if profile is not validated per ProfileQualificationMatrix.
  [[nodiscard]] static GateResult evaluateForProfile(const HardwareProfile& profile,
                                                      const float* values,
                                                      size_t values_count,
                                                      GateReport& out_report) noexcept;

  [[nodiscard]] static uint32_t requestWaiver(const char* gate_id,
                                               const char* justification) noexcept;
  [[nodiscard]] static bool approveWaiver(uint32_t waiver_id,
                                          const char* approver,
                                          uint32_t approver_id) noexcept;
  [[nodiscard]] static bool isWaiverValid(uint32_t waiver_id) noexcept;
  [[nodiscard]] static const GateWaiver* getWaiver(uint32_t waiver_id) noexcept;
  static void clearWaivers() noexcept;

  /// Mark a waiver as expired (for testing and production expiry checks).
  static bool setWaiverExpired(uint32_t waiver_id) noexcept;

  [[nodiscard]] static const ChannelPolicy* getChannelPolicy(ReleaseChannel channel) noexcept;
  [[nodiscard]] static size_t getRequiredGates(ReleaseChannel channel,
                                                std::array<char[GateRule::kMaxIdLength], 16>& out_gates) noexcept;

  static void generateGateReport(GateReport& out_report,
                                 uint8_t major, uint8_t minor, uint8_t patch) noexcept;

  static constexpr size_t kGateRulesCount = 11;

 private:
  static const std::array<GateRule, kGateRulesCount> kGateRules_;
  static std::array<GateWaiver, kMaxWaivers> waivers_;
  static size_t waiver_count_;
  static uint32_t next_waiver_id_;
};

struct GateReportSerializer {
  [[nodiscard]] static size_t serializeTo(const GateReport& report,
                                          char* buffer, size_t buffer_size) noexcept;
  [[nodiscard]] static size_t serializeEvaluationTo(const GateEvaluation& eval,
                                                     char* buffer, size_t buffer_size) noexcept;
};

}  // namespace loradriver
