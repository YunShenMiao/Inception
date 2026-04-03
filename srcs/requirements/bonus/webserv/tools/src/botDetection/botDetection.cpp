#include "../include/botDetection.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>

namespace
{
using timestamp_ms_t = BotDetection::timestamp_ms_t;

const timestamp_ms_t SIXTY_SECONDS_MS = 60000;
const timestamp_ms_t RETENTION_MS = 10 * 60 * 1000;
const size_t MAX_SAMPLES_PER_FINGERPRINT = 120;
const timestamp_ms_t ASSET_FOLLOWUP_LOOKBACK_MS = 2 * 60 * 1000;

timestamp_ms_t currentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

template <size_t N>
bool hasExtension(const std::string& ext, const char* const (&known_exts)[N])
{
    for (size_t i = 0; i < N; ++i)
    {
        if (ext == known_exts[i])
            return true;
    }
    return false;
}

BotDetection::RequestKind classifyRequestKind(const std::string& uri)
{
    const size_t query_pos = uri.find('?');
    const std::string raw_path = (query_pos == std::string::npos) ? uri : uri.substr(0, query_pos);
    const std::string path = toLower(raw_path.empty() ? std::string("/") : raw_path);

    if (path.empty() || path[path.size() - 1] == '/')
        return BotDetection::RequestKind::DOCUMENT;

    const size_t last_slash = path.find_last_of('/');
    const size_t last_dot = path.find_last_of('.');
    if (last_dot == std::string::npos || (last_slash != std::string::npos && last_dot < last_slash))
        return BotDetection::RequestKind::DOCUMENT;

    const std::string ext = path.substr(last_dot + 1);

    static const char* const asset_exts[] = {
        "css", "js", "mjs", "map",
        "png", "jpg", "jpeg", "gif", "svg", "webp", "avif", "ico", "bmp",
        "woff", "woff2", "ttf", "otf", "eot",
        "mp4", "webm", "mp3", "wav", "ogg"
    };

    if (hasExtension(ext, asset_exts))
        return BotDetection::RequestKind::ASSET;

    static const char* const document_exts[] = {
        "html", "htm", "xhtml", "php", "asp", "aspx", "jsp", "cgi", "pl", "py"
    };

    if (hasExtension(ext, document_exts))
        return BotDetection::RequestKind::DOCUMENT;

    return BotDetection::RequestKind::OTHER;
}

struct TimingStats {
    float avg_interval_ms;
    float interval_stddev_percent;
    int num_ultra_fast_consecutive;
};

TimingStats analyzeRequestTiming(
    const std::vector<timestamp_ms_t>& request_times,
    int sample_size,
    int ultra_fast_threshold_ms)
{
    TimingStats stats = {0.0f, 0.0f, 0};

    if (request_times.size() < 2)
        return stats;

    size_t start_idx = 0;
    if (request_times.size() > static_cast<size_t>(sample_size))
        start_idx = request_times.size() - static_cast<size_t>(sample_size);

    std::vector<int> intervals;
    for (size_t i = start_idx + 1; i < request_times.size(); ++i)
    {
        const int interval_ms = static_cast<int>(request_times[i] - request_times[i - 1]);
        if (interval_ms > 0)
            intervals.push_back(interval_ms);
    }

    if (intervals.empty())
        return stats;

    float sum = 0.0f;
    int consecutive_ultra_fast = 0;

    for (size_t i = 0; i < intervals.size(); ++i)
    {
        const int interval = intervals[i];
        sum += interval;

        if (interval < ultra_fast_threshold_ms)
        {
            ++consecutive_ultra_fast;
            if (consecutive_ultra_fast > stats.num_ultra_fast_consecutive)
                stats.num_ultra_fast_consecutive = consecutive_ultra_fast;
        }
        else
        {
            consecutive_ultra_fast = 0;
        }
    }

    stats.avg_interval_ms = sum / static_cast<float>(intervals.size());

    float sum_sq_diff = 0.0f;
    for (size_t i = 0; i < intervals.size(); ++i)
    {
        const float diff = intervals[i] - stats.avg_interval_ms;
        sum_sq_diff += diff * diff;
    }
    const float variance = sum_sq_diff / static_cast<float>(intervals.size());
    const float stddev = std::sqrt(variance);

    if (stats.avg_interval_ms > 0.0f)
        stats.interval_stddev_percent = (stddev / stats.avg_interval_ms) * 100.0f;

    return stats;
}

std::vector<timestamp_ms_t> collectPrimaryRequestTimes(
    const std::vector<BotDetection::RequestSample>& samples)
{
    std::vector<timestamp_ms_t> primary_times;
    primary_times.reserve(samples.size());

    for (size_t i = 0; i < samples.size(); ++i)
    {
        if (samples[i].kind != BotDetection::RequestKind::ASSET)
            primary_times.push_back(samples[i].timestamp_ms);
    }
    return primary_times;
}

int countRequestsInLastMinute(const std::vector<timestamp_ms_t>& times, timestamp_ms_t now)
{
    int count = 0;
    for (size_t i = 0; i < times.size(); ++i)
    {
        const timestamp_ms_t ts = times[i];
        if (ts <= now && now - ts <= SIXTY_SECONDS_MS)
            ++count;
    }
    return count;
}

struct AssetFollowupStats {
    int mature_document_count;
    int missing_followup_count;
};

AssetFollowupStats analyzeAssetFollowup(
    const std::vector<BotDetection::RequestSample>& samples,
    timestamp_ms_t now,
    int followup_window_ms)
{
    AssetFollowupStats stats = {0, 0};
    const timestamp_ms_t lower_bound = now - ASSET_FOLLOWUP_LOOKBACK_MS;
    const timestamp_ms_t followup_window = static_cast<timestamp_ms_t>(followup_window_ms);

    for (size_t i = 0; i < samples.size(); ++i)
    {
        const BotDetection::RequestSample& sample = samples[i];
        if (sample.kind != BotDetection::RequestKind::DOCUMENT)
            continue;
        if (sample.timestamp_ms < lower_bound)
            continue;

        const timestamp_ms_t age = now - sample.timestamp_ms;
        if (age <= followup_window)
            continue;

        ++stats.mature_document_count;

        bool has_asset_followup = false;
        for (size_t j = i + 1; j < samples.size(); ++j)
        {
            const BotDetection::RequestSample& next = samples[j];
            const timestamp_ms_t delta = next.timestamp_ms - sample.timestamp_ms;
            if (delta > followup_window)
                break;
            if (next.kind == BotDetection::RequestKind::ASSET)
            {
                has_asset_followup = true;
                break;
            }
        }

        if (!has_asset_followup)
            ++stats.missing_followup_count;
    }

    return stats;
}

void trimRequestHistory(
    std::vector<BotDetection::RequestSample>& samples,
    timestamp_ms_t now,
    timestamp_ms_t retention_ms,
    size_t max_samples)
{
    size_t keep_from = 0;
    while (keep_from < samples.size() &&
           now - samples[keep_from].timestamp_ms > retention_ms)
    {
        ++keep_from;
    }
    if (keep_from > 0)
    {
        const std::vector<BotDetection::RequestSample>::difference_type erase_count =
            static_cast<std::vector<BotDetection::RequestSample>::difference_type>(keep_from);
        samples.erase(samples.begin(), samples.begin() + erase_count);
    }

    if (samples.size() > max_samples)
    {
        const size_t extra = samples.size() - max_samples;
        const std::vector<BotDetection::RequestSample>::difference_type erase_count =
            static_cast<std::vector<BotDetection::RequestSample>::difference_type>(extra);
        samples.erase(samples.begin(), samples.begin() + erase_count);
    }
}

BotDetection::BotAnalysis analyzeRequest(
    const std::vector<BotDetection::RequestSample>& samples,
    const BotDetection::BotDetectionConfig& config)
{
    BotDetection::BotAnalysis analysis = {
        BotDetection::BotScore::CLEAN, 0, 0, 0.0f, 0.0f, ""
    };

    const timestamp_ms_t now = currentTimeMs();
    const std::vector<timestamp_ms_t> primary_times = collectPrimaryRequestTimes(samples);

    analysis.requests_per_minute = countRequestsInLastMinute(primary_times, now);

    if (analysis.requests_per_minute > config.max_requests_per_minute)
    {
        analysis.score = BotDetection::BotScore::BLOCKED;
        analysis.pattern_type = "RATE_LIMIT_EXCEEDED";
        analysis.suspicious_score = 100;
        return analysis;
    }

    const int threshold = static_cast<int>(
        config.max_requests_per_minute * config.rate_limit_threshold_percent);
    if (analysis.requests_per_minute > threshold)
        analysis.suspicious_score += config.score_mechanical;

    const TimingStats timing = analyzeRequestTiming(
        primary_times, config.sample_size, config.ultra_fast_threshold_ms);

    analysis.avg_request_interval_ms = timing.avg_interval_ms;
    analysis.interval_stddev_percent = timing.interval_stddev_percent;

    if (timing.avg_interval_ms >= config.mechanical_interval_min_ms &&
        timing.avg_interval_ms <= config.mechanical_interval_max_ms &&
        timing.interval_stddev_percent < config.mechanical_stddev_threshold_percent)
    {
        analysis.pattern_type = "MECHANICAL_INTERVALS";
        analysis.suspicious_score += config.score_mechanical;
    }

    if (timing.num_ultra_fast_consecutive >= config.burst_consecutive_count)
    {
        if (analysis.pattern_type.empty())
            analysis.pattern_type = "ULTRA_FAST_BURST";
        analysis.suspicious_score += config.score_ultra_fast;
    }

    if (timing.avg_interval_ms > config.mechanical_interval_min_ms &&
        timing.avg_interval_ms <= 300.0f &&
        timing.interval_stddev_percent < 50.0f)
    {
        if (analysis.pattern_type.empty() || analysis.pattern_type != "MECHANICAL_INTERVALS")
        {
            analysis.pattern_type = "PREDICTABLE_SCANNING";
            analysis.suspicious_score += config.score_predictable;
        }
    }

    const AssetFollowupStats followup = analyzeAssetFollowup(
        samples, now, config.asset_followup_window_ms);

    if (followup.mature_document_count >= config.min_document_samples_for_asset_check)
    {
        const float missing_ratio_percent =
            (100.0f * followup.missing_followup_count) /
            static_cast<float>(followup.mature_document_count);

        if (missing_ratio_percent >= config.missing_asset_ratio_threshold_percent)
        {
            if (analysis.pattern_type.empty())
                analysis.pattern_type = "MISSING_ASSET_FOLLOWUP";
            analysis.suspicious_score += config.score_missing_asset_followup;
        }

        if (followup.mature_document_count >= config.document_only_block_min_samples &&
            missing_ratio_percent >= config.document_only_block_ratio_threshold_percent)
        {
            analysis.pattern_type = "DOCUMENT_ONLY_PATTERN";
            analysis.suspicious_score += config.score_document_only_block;
        }
    }

    if (timing.avg_interval_ms > 1000.0f &&
        timing.interval_stddev_percent > 50.0f)
    {
        analysis.pattern_type = "HUMAN_LIKE";
        analysis.suspicious_score = std::max(0, analysis.suspicious_score - 10);
    }

    if (analysis.suspicious_score >= config.score_blocked_threshold)
        analysis.score = BotDetection::BotScore::BLOCKED;
    else if (analysis.suspicious_score >= config.score_suspicious_threshold)
        analysis.score = BotDetection::BotScore::SUSPICIOUS;
    else
        analysis.score = BotDetection::BotScore::CLEAN;

    return analysis;
}
}

namespace BotDetection {

BotDetectionConfig getDefaultConfig()
{
    return BotDetectionConfig();
}

BotAnalysis analyzeAndTrackRequest(
    const std::string& fingerprint,
    const std::string& uri,
    std::unordered_map<std::string, std::vector<RequestSample>>& request_history,
    const BotDetectionConfig& config)
{
    const timestamp_ms_t now = currentTimeMs();
    std::vector<RequestSample>& samples = request_history[fingerprint];
    samples.push_back(RequestSample{now, classifyRequestKind(uri)});

    trimRequestHistory(samples, now, RETENTION_MS, MAX_SAMPLES_PER_FINGERPRINT);

    return ::analyzeRequest(samples, config);
}

}  // namespace BotDetection
