#ifndef BOT_DETECTION_HPP
#define BOT_DETECTION_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Bot Detection Module
 * Analyzes request patterns to detect automated traffic
 */

namespace BotDetection {
using timestamp_ms_t = std::int64_t;

enum class RequestKind {
    DOCUMENT = 0,
    ASSET = 1,
    OTHER = 2
};

struct RequestSample {
    timestamp_ms_t timestamp_ms;
    RequestKind kind;
};

/**
 * Bot Detection Score
 */
enum class BotScore {
    CLEAN = 0,        // Legitimate traffic
    SUSPICIOUS = 1,   // Suspicious pattern, allow but monitor
    BLOCKED = 2       // Bot behavior detected, block with HTTP 429
};

/**
 * Result of bot analysis
 */
struct BotAnalysis {
    BotScore score;                    // Detection verdict
    int requests_per_minute;           // Calculated non-asset RPM in last 60 seconds
    int suspicious_score;              // Accumulated suspicion score (0-100)
    float avg_request_interval_ms;     // Average milliseconds between requests
    float interval_stddev_percent;     // Standard deviation as % of average
    std::string pattern_type;          // Type of pattern detected (if any)
};

/**
 * Configuration for bot detection behavior
 */
struct BotDetectionConfig {
    // Rate limiting settings
    int max_requests_per_minute = 60;
    float rate_limit_threshold_percent = 0.75f;  // 75% = 45 requests
    
    // Timing pattern analysis
    int sample_size = 10;                        // Analyze last 10 requests
    int mechanical_interval_min_ms = 100;        // Pattern must be 100ms+
    int mechanical_interval_max_ms = 500;        // Pattern must be <=500ms
    float mechanical_stddev_threshold_percent = 10.0f;  // < 10% stddev
    int ultra_fast_threshold_ms = 100;           // < 100ms = ultra-fast
    int burst_consecutive_count = 3;             // 3+ ultra-fast = burst

    // Asset-followup behavior analysis
    int asset_followup_window_ms = 2500;         // Asset requests expected shortly after documents
    int min_document_samples_for_asset_check = 3;
    float missing_asset_ratio_threshold_percent = 80.0f;
    int document_only_block_min_samples = 6;     // Strong document-only pattern threshold
    float document_only_block_ratio_threshold_percent = 95.0f;
    
    // Scoring thresholds
    int score_mechanical = 30;
    int score_ultra_fast = 20;
    int score_predictable = 15;
    int score_missing_asset_followup = 15;
    int score_document_only_block = 50;
    int score_blocked_threshold = 50;
    int score_suspicious_threshold = 25;
    
    // Block duration
    int block_duration_seconds = 300;  // 5 minute blocks
};

/**
 * Track current request timestamp and run bot analysis for this fingerprint
 * @param fingerprint Client fingerprint key
 * @param uri Requested URI
 * @param request_history Fingerprint -> request sample history store
 * @param config Detection configuration
 * @return BotAnalysis with verdict and metrics
 */
BotAnalysis analyzeAndTrackRequest(
    const std::string& fingerprint,
    const std::string& uri,
    std::unordered_map<std::string, std::vector<RequestSample>>& request_history,
    const BotDetectionConfig& config
);

/**
 * Get default configuration
 */
BotDetectionConfig getDefaultConfig();

}  // namespace BotDetection

#endif // BOT_DETECTION_HPP
