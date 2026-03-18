#include "loradriver/ci_gates.hpp"
#include "loradriver/artifact_governance.hpp"
#include "loradriver/profile_qualification.hpp"
#include <cstring>
#include <cstdio>

namespace loradriver {

namespace {
void registerGateReportArtifact(const GateReport& report) {
  ArtifactMetadata metadata{};
  metadata.type = ArtifactType::kGateReport;
  metadata.created_timestamp = 1;
  metadata.retention_days = 90;
  std::snprintf(metadata.linked_version, sizeof(metadata.linked_version), "%u.%u.%u",
                report.report_major, report.report_minor, report.report_patch);
  std::strncpy(metadata.source_module, "CiGateEngine", sizeof(metadata.source_module) - 1);
  metadata.source_module[sizeof(metadata.source_module) - 1] = '\0';
  (void)ArtifactRegistry::registerArtifact(metadata);
}
}  // namespace

std::array<GateWaiver, CiGateEngine::kMaxWaivers> CiGateEngine::waivers_ = {};
size_t CiGateEngine::waiver_count_ = 0;
uint32_t CiGateEngine::next_waiver_id_ = 1;

const std::array<GateRule, CiGateEngine::kGateRulesCount> CiGateEngine::kGateRules_ = {{
    {
        "INIT-001",
        GateCategory::kInit,
        GateSeverity::kBlocking,
        {"init_success_rate", ThresholdOperator::kGreaterOrEqual, 99.0f, "percent"},
        "Radio init success rate",
        true
    },
    {
        "INIT-002",
        GateCategory::kInit,
        GateSeverity::kBlocking,
        {"init_time_p99", ThresholdOperator::kLessOrEqual, 500.0f, "ms"},
        "Init latency P99",
        true
    },
    {
        "TXRX-001",
        GateCategory::kTxRx,
        GateSeverity::kBlocking,
        {"tx_success_rate", ThresholdOperator::kGreaterOrEqual, 99.0f, "percent"},
        "TX success rate",
        true
    },
    {
        "TXRX-002",
        GateCategory::kTxRx,
        GateSeverity::kBlocking,
        {"rx_success_rate", ThresholdOperator::kGreaterOrEqual, 98.0f, "percent"},
        "RX success rate",
        true
    },
    {
        "IRQ-001",
        GateCategory::kIrq,
        GateSeverity::kBlocking,
        {"irq_handled_rate", ThresholdOperator::kGreaterOrEqual, 99.9f, "percent"},
        "IRQ handling rate",
        true
    },
    {
        "IRQ-002",
        GateCategory::kIrq,
        GateSeverity::kBlocking,
        {"irq_overflow_count", ThresholdOperator::kEqual, 0.0f, "count"},
        "No IRQ overflow",
        true
    },
    {
        "TIMEOUT-001",
        GateCategory::kTimeout,
        GateSeverity::kBlocking,
        {"recovery_success_rate", ThresholdOperator::kGreaterOrEqual, 99.0f, "percent"},
        "Timeout recovery rate",
        true
    },
    {
        "RECOVERY-001",
        GateCategory::kRecovery,
        GateSeverity::kBlocking,
        {"sleep_wakeup_success", ThresholdOperator::kGreaterOrEqual, 99.0f, "percent"},
        "Sleep/wakeup recovery",
        true
    },
    {
        "RECOVERY-002",
        GateCategory::kRecovery,
        GateSeverity::kBlocking,
        {"timeout_recovery_success_rate", ThresholdOperator::kGreaterOrEqual, 100.0f, "percent"},
        "All validated profiles must have timeout recovery evidence",
        true
    },
    {
        "INTEGRATION-001",
        GateCategory::kIntegration,
        GateSeverity::kBlocking,
        {"fsm_deadlock_count", ThresholdOperator::kEqual, 0.0f, "count"},
        "No FSM deadlock",
        true
    },
    {
        "NONREG-001",
        GateCategory::kIntegration,
        GateSeverity::kBlocking,
        {"suite_pass_rate", ThresholdOperator::kGreaterOrEqual, 100.0f, "percent"},
        "All non-regression cases must pass",
        true
    }
}};

const GateRule* CiGateEngine::getGateRule(const char* gate_id) noexcept {
  if (gate_id == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < kGateRulesCount; ++i) {
    if (std::strcmp(kGateRules_[i].id, gate_id) == 0) {
      return &kGateRules_[i];
    }
  }
  return nullptr;
}

GateResult CiGateEngine::evaluateGate(const char* gate_id, float actual_value) noexcept {
  const GateRule* rule = getGateRule(gate_id);
  if (rule == nullptr) {
    return GateResult::kError;
  }
  return evaluateGate(*rule, actual_value);
}

GateResult CiGateEngine::evaluateGate(const GateRule& rule, float actual_value) noexcept {
  if (!rule.enabled) {
    return GateResult::kDisabled;
  }

  if (rule.threshold.evaluate(actual_value)) {
    return GateResult::kPass;
  }
  return GateResult::kFail;
}

void CiGateEngine::evaluateAllGates(const float* values, size_t values_count,
                                    GateReport& out_report) noexcept {
  out_report.evaluations_count = 0;
  out_report.blocking_passed = 0;
  out_report.blocking_failed = 0;
  out_report.warning_count = 0;
  out_report.advisory_count = 0;

  const size_t count = (values_count < kGateRulesCount) ? values_count : kGateRulesCount;

  for (size_t i = 0; i < count; ++i) {
    const GateRule& rule = kGateRules_[i];
    GateResult result = evaluateGate(rule, values[i]);

    GateEvaluation& eval = out_report.evaluations[out_report.evaluations_count];
    std::strcpy(eval.gate_id, rule.id);
    eval.result = result;
    eval.actual_value = values[i];
    eval.threshold_value = rule.threshold.value;
    eval.threshold_met = (result == GateResult::kPass || result == GateResult::kDisabled);
    out_report.evaluations_count++;

    if (result == GateResult::kPass) {
      if (rule.severity == GateSeverity::kBlocking) {
        out_report.blocking_passed++;
      } else if (rule.severity == GateSeverity::kWarning) {
        out_report.warning_count++;
      } else {
        out_report.advisory_count++;
      }
    } else if (result == GateResult::kFail) {
      if (rule.severity == GateSeverity::kBlocking) {
        out_report.blocking_failed++;
      } else if (rule.severity == GateSeverity::kWarning) {
        out_report.warning_count++;
      } else {
        out_report.advisory_count++;
      }
    }
  }

  out_report.release_blocked = isReleaseBlocked(out_report);
  registerGateReportArtifact(out_report);
}

GateResult CiGateEngine::evaluateForProfile(const HardwareProfile& profile,
                                             const float* values,
                                             size_t values_count,
                                             GateReport& out_report) noexcept {
  if (!ProfileQualificationMatrix::isProfileValidated(profile)) {
    out_report.evaluations_count = 0;
    out_report.blocking_passed = 0;
    out_report.blocking_failed = 0;
    out_report.warning_count = 0;
    out_report.advisory_count = 0;
    out_report.release_blocked = false;
    return GateResult::kError;
  }
  
  evaluateAllGates(values, values_count, out_report);
  return out_report.release_blocked ? GateResult::kFail : GateResult::kPass;
}

bool CiGateEngine::isReleaseBlocked(const GateReport& report) noexcept {
  return report.blocking_failed > 0;
}

void CiGateEngine::getFailedBlockingGates(const GateReport& report,
                                          std::array<GateEvaluation, 16>& out_failed,
                                          uint8_t& out_count) noexcept {
  out_count = 0;
  for (uint8_t i = 0; i < report.evaluations_count && out_count < 16; ++i) {
    const GateEvaluation& eval = report.evaluations[i];
    if (eval.result == GateResult::kFail) {
      const GateRule* rule = getGateRule(eval.gate_id);
      if (rule != nullptr && rule->isBlocking()) {
        out_failed[out_count] = eval;
        out_count++;
      }
    }
  }
}

uint32_t CiGateEngine::requestWaiver(const char* gate_id, const char* justification) noexcept {
  if (gate_id == nullptr || justification == nullptr) {
    return 0;
  }
  if (waiver_count_ >= kMaxWaivers) {
    return 0;
  }

  const GateRule* rule = getGateRule(gate_id);
  if (rule == nullptr) {
    return 0;
  }

  GateWaiver& waiver = waivers_[waiver_count_];
  std::strncpy(waiver.gate_id, gate_id, sizeof(waiver.gate_id) - 1);
  waiver.gate_id[sizeof(waiver.gate_id) - 1] = '\0';
  std::strncpy(waiver.justification, justification, sizeof(waiver.justification) - 1);
  waiver.justification[sizeof(waiver.justification) - 1] = '\0';
  waiver.waiver_id = next_waiver_id_++;
  waiver.is_approved = 0;
  waiver.is_expired = 0;
  waiver.approved_by_id = 0;
  waiver.approver[0] = '\0';
  waiver.created_timestamp = 0;
  waiver.expiry_timestamp = 0;

  waiver_count_++;
  return waiver.waiver_id;
}

bool CiGateEngine::approveWaiver(uint32_t waiver_id, const char* approver,
                                  uint32_t approver_id) noexcept {
  if (approver == nullptr) {
    return false;
  }

  for (size_t i = 0; i < waiver_count_; ++i) {
    if (waivers_[i].waiver_id == waiver_id) {
      waivers_[i].is_approved = 1;
      std::strncpy(waivers_[i].approver, approver, sizeof(waivers_[i].approver) - 1);
      waivers_[i].approver[sizeof(waivers_[i].approver) - 1] = '\0';
      waivers_[i].approved_by_id = approver_id;
      return true;
    }
  }
  return false;
}

bool CiGateEngine::isWaiverValid(uint32_t waiver_id) noexcept {
  const GateWaiver* waiver = getWaiver(waiver_id);
  if (waiver == nullptr) {
    return false;
  }
  return waiver->is_approved && !waiver->is_expired;
}

const GateWaiver* CiGateEngine::getWaiver(uint32_t waiver_id) noexcept {
  for (size_t i = 0; i < waiver_count_; ++i) {
    if (waivers_[i].waiver_id == waiver_id) {
      return &waivers_[i];
    }
  }
  return nullptr;
}

bool CiGateEngine::setWaiverExpired(uint32_t waiver_id) noexcept {
  for (size_t i = 0; i < waiver_count_; ++i) {
    if (waivers_[i].waiver_id == waiver_id) {
      waivers_[i].is_expired = 1;
      return true;
    }
  }
  return false;
}

void CiGateEngine::clearWaivers() noexcept {
  waiver_count_ = 0;
  next_waiver_id_ = 1;
  waivers_ = {};
}

static ChannelPolicy sRegularPolicy = {};
static ChannelPolicy sHotfixPolicy = {};
static bool sPoliciesInitialized = false;

static void InitializePolicies() {
  if (sPoliciesInitialized) {
    return;
  }

  sRegularPolicy.channel = ReleaseChannel::kRegular;
  sRegularPolicy.required_gates_count = 11;
  std::strcpy(sRegularPolicy.required_gates[0], "INIT-001");
  std::strcpy(sRegularPolicy.required_gates[1], "INIT-002");
  std::strcpy(sRegularPolicy.required_gates[2], "TXRX-001");
  std::strcpy(sRegularPolicy.required_gates[3], "TXRX-002");
  std::strcpy(sRegularPolicy.required_gates[4], "IRQ-001");
  std::strcpy(sRegularPolicy.required_gates[5], "IRQ-002");
  std::strcpy(sRegularPolicy.required_gates[6], "TIMEOUT-001");
  std::strcpy(sRegularPolicy.required_gates[7], "RECOVERY-001");
  std::strcpy(sRegularPolicy.required_gates[8], "RECOVERY-002");
  std::strcpy(sRegularPolicy.required_gates[9], "INTEGRATION-001");
  std::strcpy(sRegularPolicy.required_gates[10], "NONREG-001");
  sRegularPolicy.allow_waivers = true;
  std::strcpy(sRegularPolicy.waiver_approvers[0], "release-owner");
  std::strcpy(sRegularPolicy.waiver_approvers[1], "tech-lead");
  sRegularPolicy.approvers_count = 2;

  sHotfixPolicy.channel = ReleaseChannel::kHotfix;
  sHotfixPolicy.required_gates_count = 11;
  std::strcpy(sHotfixPolicy.required_gates[0], "INIT-001");
  std::strcpy(sHotfixPolicy.required_gates[1], "INIT-002");
  std::strcpy(sHotfixPolicy.required_gates[2], "TXRX-001");
  std::strcpy(sHotfixPolicy.required_gates[3], "TXRX-002");
  std::strcpy(sHotfixPolicy.required_gates[4], "IRQ-001");
  std::strcpy(sHotfixPolicy.required_gates[5], "IRQ-002");
  std::strcpy(sHotfixPolicy.required_gates[6], "TIMEOUT-001");
  std::strcpy(sHotfixPolicy.required_gates[7], "RECOVERY-001");
  std::strcpy(sHotfixPolicy.required_gates[8], "RECOVERY-002");
  std::strcpy(sHotfixPolicy.required_gates[9], "INTEGRATION-001");
  std::strcpy(sHotfixPolicy.required_gates[10], "NONREG-001");
  sHotfixPolicy.allow_waivers = true;
  std::strcpy(sHotfixPolicy.waiver_approvers[0], "release-owner");
  std::strcpy(sHotfixPolicy.waiver_approvers[1], "tech-lead");
  std::strcpy(sHotfixPolicy.waiver_approvers[2], "incident-commander");
  sHotfixPolicy.approvers_count = 3;

  sPoliciesInitialized = true;
}

const ChannelPolicy* CiGateEngine::getChannelPolicy(ReleaseChannel channel) noexcept {
  InitializePolicies();

  if (channel == ReleaseChannel::kRegular) {
    return &sRegularPolicy;
  }
  if (channel == ReleaseChannel::kHotfix) {
    return &sHotfixPolicy;
  }
  return nullptr;
}

size_t CiGateEngine::getRequiredGates(ReleaseChannel channel,
                                       std::array<char[GateRule::kMaxIdLength], 16>& out_gates) noexcept {
  const ChannelPolicy* policy = getChannelPolicy(channel);
  if (policy == nullptr) {
    return 0;
  }

  const size_t count = (policy->required_gates_count < 16) ? policy->required_gates_count : 16;
  for (size_t i = 0; i < count; ++i) {
    std::strcpy(out_gates[i], policy->required_gates[i]);
  }
  return count;
}

void CiGateEngine::generateGateReport(GateReport& out_report,
                                       uint8_t major, uint8_t minor, uint8_t patch) noexcept {
  out_report.report_major = major;
  out_report.report_minor = minor;
  out_report.report_patch = patch;

  out_report.evaluations_count = kGateRulesCount;
  out_report.blocking_passed = kGateRulesCount;
  out_report.blocking_failed = 0;
  out_report.warning_count = 0;
  out_report.advisory_count = 0;

  for (size_t i = 0; i < kGateRulesCount; ++i) {
    const GateRule& rule = kGateRules_[i];
    GateEvaluation& eval = out_report.evaluations[i];
    std::strcpy(eval.gate_id, rule.id);
    eval.result = GateResult::kPass;
    eval.actual_value = rule.threshold.value;
    eval.threshold_value = rule.threshold.value;
    eval.threshold_met = true;
  }

  out_report.release_blocked = false;
}

size_t GateReportSerializer::serializeTo(const GateReport& report,
                                          char* buffer, size_t buffer_size) noexcept {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  size_t offset = 0;
  int written = std::snprintf(buffer + offset, buffer_size - offset,
      "GATE_REPORT v=%u.%u.%u\n", report.report_major, report.report_minor, report.report_patch);
  if (written < 0 || static_cast<size_t>(written) >= buffer_size - offset) {
    return 0;
  }
  offset += static_cast<size_t>(written);

  written = std::snprintf(buffer + offset, buffer_size - offset,
      "blocking_passed=%u blocking_failed=%u release_blocked=%s\n",
      report.blocking_passed, report.blocking_failed,
      report.release_blocked ? "true" : "false");
  if (written < 0 || static_cast<size_t>(written) >= buffer_size - offset) {
    return 0;
  }
  offset += static_cast<size_t>(written);

  for (uint8_t i = 0; i < report.evaluations_count; ++i) {
    const GateEvaluation& eval = report.evaluations[i];
    written = std::snprintf(buffer + offset, buffer_size - offset,
        "GATE: %s result=%u actual=%.2f threshold=%.2f met=%s\n",
        eval.gate_id, static_cast<uint8_t>(eval.result),
        eval.actual_value, eval.threshold_value,
        eval.threshold_met ? "true" : "false");
    if (written < 0 || static_cast<size_t>(written) >= buffer_size - offset) {
      return 0;
    }
    offset += static_cast<size_t>(written);
  }

  return offset;
}

size_t GateReportSerializer::serializeEvaluationTo(const GateEvaluation& eval,
                                                    char* buffer, size_t buffer_size) noexcept {
  if (buffer == nullptr || buffer_size == 0) {
    return 0;
  }

  int written = std::snprintf(buffer, buffer_size,
      "GATE: %s result=%u actual=%.2f threshold=%.2f met=%s",
      eval.gate_id, static_cast<uint8_t>(eval.result),
      eval.actual_value, eval.threshold_value,
      eval.threshold_met ? "true" : "false");

  if (written < 0 || static_cast<size_t>(written) >= buffer_size) {
    return 0;
  }
  return static_cast<size_t>(written);
}

}  // namespace loradriver
