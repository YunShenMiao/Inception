#ifndef STATUSCODES_HPP
#define STATUSCODES_HPP

enum ErrorCode
{
    BadRequest = 400,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    PayloadTooLarge = 413,
    URITooLong = 414,
    TooManyRequests = 429, // Bot detection rate limit
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    GatewayTimeout = 504,
    HTTPVersionNotSupported = 505
};

enum SuccessCode
{
    Ok = 200,
    Created = 201,
    NoContent = 204
};

enum RedirectCode
{
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303
};

#endif
