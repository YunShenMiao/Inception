#include "../../include/captchaBypass.hpp"

#include <limits>
#include <random>

namespace
{
std::string trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
        ++start;

    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        --end;

    return s.substr(start, end - start);
}
}

CaptchaBypass::CaptchaBypass(size_t max_entries, std::time_t ttl_seconds):
    token_store(),
    max_entries(max_entries),
    ttl_seconds(ttl_seconds)
{
}

std::string CaptchaBypass::generateToken() const
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<unsigned long long> dist(
        1ULL, std::numeric_limits<unsigned long long>::max());

    return std::to_string(dist(gen));
}

void CaptchaBypass::cleanupExpired()
{
    const std::time_t now = std::time(nullptr);

    for (std::unordered_map<std::string, Entry>::iterator it = token_store.begin(); it != token_store.end();)
    {
        if (now - it->second.last_seen > ttl_seconds)
            it = token_store.erase(it);
        else
            ++it;
    }
}

void CaptchaBypass::evictOldestIfNeeded()
{
    if (max_entries == 0)
    {
        token_store.clear();
        return;
    }

    while (token_store.size() >= max_entries)
    {
        std::unordered_map<std::string, Entry>::iterator oldest = token_store.begin();
        for (std::unordered_map<std::string, Entry>::iterator it = token_store.begin(); it != token_store.end(); ++it)
        {
            if (it->second.last_seen < oldest->second.last_seen)
                oldest = it;
        }
        token_store.erase(oldest);
    }
}

std::string CaptchaBypass::extractTokenFromCookie(const std::string& cookie_header) const
{
    const std::string key = "catsurf_clearance=";
    size_t start = 0;

    while (start < cookie_header.size())
    {
        size_t end = cookie_header.find(';', start);
        if (end == std::string::npos)
            end = cookie_header.size();

        std::string part = trim(cookie_header.substr(start, end - start));
        if (part.compare(0, key.size(), key) == 0)
            return part.substr(key.size());

        if (end == cookie_header.size())
            break;
        start = end + 1;
    }

    return "";
}

bool CaptchaBypass::hasValidBypass(const std::string& token, const std::string& fingerprint)
{
    if (token.empty())
        return false;

    cleanupExpired();

    std::unordered_map<std::string, Entry>::iterator it = token_store.find(token);
    if (it == token_store.end())
        return false;

    if (it->second.fingerprint != fingerprint)
        return false;

    it->second.last_seen = std::time(nullptr);
    return true;
}

std::string CaptchaBypass::createBypass(const std::string& fingerprint)
{
    cleanupExpired();
    evictOldestIfNeeded();

    std::string token = generateToken();
    for (int i = 0; i < 8 && token_store.find(token) != token_store.end(); ++i)
        token = generateToken();

    const std::time_t now = std::time(nullptr);
    Entry entry = { fingerprint, now };
    token_store[token] = entry;
    return token;
}

bool CaptchaBypass::isSolvedCaptchaPost(const std::string& method, const std::string& uri, const std::string& body) const
{
    if (method != "POST" || uri != "/")
        return false;

    return body.find("captcha=1") != std::string::npos ||
           body.find("captcha=on") != std::string::npos ||
           body.find("captcha=true") != std::string::npos;
}

std::string CaptchaBypass::buildCaptchaPage() const
{
    return "<!DOCTYPE html>"
       "<html lang=\"de\">"
       "<head>"
       "<meta charset=\"UTF-8\">"
       "<title>429 - Too Many Meowquests</title>"
       "<style>"
       "body{background:linear-gradient(135deg,#ffe6f2,#fff8dc);font-family:Arial,sans-serif;text-align:center;padding-top:80px;}"
       ".card{background:white;display:inline-block;padding:40px;border-radius:20px;"
       "box-shadow:0 10px 30px rgba(0,0,0,0.1);max-width:420px;}"
       "h1{font-size:26px;color:#cc3366;}"
       ".cat{font-size:70px;}"
       "button{background-color:#ff66a3;border:none;color:white;padding:12px 25px;"
       "font-size:16px;border-radius:30px;cursor:pointer;transition:0.3s;}"
       "button:hover{background-color:#e0558f;}"
       "label{font-size:15px;}"
       ".small{font-size:13px;color:#777;margin-top:15px;}"
       "</style>"
       "</head>"
       "<body>"
       "<div class=\"card\">"
       "<div class=\"cat\">🐾😼</div>"
       "<h1>HTTP 429 – Too Many Meowquests!</h1>"
       "<p>Whoa there, Tiger! 🐅<br>"
       "To many request! ARE YOU CATBOT?!<br>"
       "Please solve the captcha to continue surfing.</p>"
       "<form method=\"POST\" action=\"/\">"
       "<label><input type=\"checkbox\" name=\"captcha\" value=\"1\" required> I am not a Catbot 🐱</label>"
       "<br><br>"
       "<button type=\"submit\">Continue surfing</button>"
       "</form>"
       "<div class=\"small\">Notice: Cats needs rest to. 💤</div>"
       "</div>"
       "</body>"
       "</html>";
}
