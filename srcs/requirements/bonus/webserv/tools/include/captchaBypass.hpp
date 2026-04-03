#ifndef CAPTCHA_BYPASS_HPP
#define CAPTCHA_BYPASS_HPP

#include <cstddef>
#include <ctime>
#include <string>
#include <unordered_map>

class CaptchaBypass
{
    private:
    struct Entry
    {
        std::string fingerprint;
        std::time_t last_seen;
    };

    std::unordered_map<std::string, Entry> token_store;
    size_t max_entries;
    std::time_t ttl_seconds;

    std::string generateToken() const;
    void cleanupExpired();
    void evictOldestIfNeeded();

    public:
    CaptchaBypass(size_t max_entries = 4096, std::time_t ttl_seconds = 3600);

    std::string extractTokenFromCookie(const std::string& cookie_header) const;
    bool hasValidBypass(const std::string& token, const std::string& fingerprint);
    std::string createBypass(const std::string& fingerprint);
    bool isSolvedCaptchaPost(const std::string& method, const std::string& uri, const std::string& body) const;
    std::string buildCaptchaPage() const;
};

#endif
