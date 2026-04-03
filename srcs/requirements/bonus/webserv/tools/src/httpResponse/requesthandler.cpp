
#include "../../include/requestHandler.hpp"
#include "../../include/utils.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <cstdio>

RequestHandler::RequestHandler(const Route &r, const parsedRequest &pr, const ServerConfig &sc, bool keep_alive): r(r), pr(pr), sc(sc) 
{
    if (keep_alive)
        ka = "keep-alive";
    else 
        ka = "close";
}

RequestHandler::~RequestHandler() {}

HttpResponse RequestHandler::handle()
{
    size_t cmb;
    if (r.location)
        cmb = r.location->client_max_body_size;
    else 
        cmb = sc.client_max_body_size;
    if (cmb > 0 && pr.body.size() > cmb)
        return handleError(PayloadTooLarge);
    switch (r.type)
    {
        case RED:
            return handleRedirect();
        case CGI:
            return handleCGI();
        case FILES:
            return handleFiles();
        case UPLOAD:
            return handleFiles();
        case DIRECTORY_LISTING:
            return handleDirectoryListing();
        default:
            return handleError(r.status);
    }
}

HttpResponse RequestHandler::handleCGI() 
{
    HttpResponse res(ka, pr.http_v);
    return res;
}

HttpResponse RequestHandler::handleDirectoryListing()
{
    std::string body;
    std::string dirName = pr.uri;

    if (dirName.back() != '/')
        dirName += '/';

    body = "<!DOCTYPE html>\n<html>\n<head>\n<title>Index of " + htmlEscape(dirName)
            + "</title>\n</head>\n<body>\n<h1>Index of " + htmlEscape(dirName) + "</h1>\n<ul>\n";
    try
    {
        if (dirName != "/") 
            body += "<li><a href=\"../\">Parent_Directory</a></li>\n";
        for (const auto & entry : std::filesystem::directory_iterator(r.file_path))
        {    
            std::string fileName = entry.path().filename().string();
            if (fileName[0] == '.')
                continue;
            if (entry.is_directory())
                fileName += "/";
            body += "<li><a href=\"" + htmlEscape(dirName + fileName) + "\">" + htmlEscape(fileName) + "</a></li>\n";
        }
            body += "  </ul>\n</body>\n</html>\r\n";
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        return handleError(Forbidden);
    }

    HttpResponse res(ka, pr.http_v);
    res.setHeader("Content-Type", "text/html; charset=utf-8");
    res.setHeader("Content-Length", std::to_string(body.size()));
    res.setStatus(Ok);
    res.setBody(body);
    return res;
}

std::string extractFile(std::string filePath)
{
    size_t sep = filePath.find_last_of("/");
    if (sep != std::string::npos)
        return filePath.substr(sep + 1);
    return "";
}

HttpResponse RequestHandler::deleteFile()
{
    if (!r.location)
        return handleError(NotFound);
    if (r.location->upload_path.empty())
        return handleError(MethodNotAllowed);
    else
    {
        std::string fileName = extractFile(pr.uri);
        if (fileName.empty())
            return handleError(NotFound);
        if (fileName == "." || fileName == ".." || fileName.find('\\') != std::string::npos)
            return handleError(BadRequest);
        std::string uploadPath = r.location->upload_path;
        if (uploadPath.back() != '/')
        uploadPath += '/';

        std::string fullPath = uploadPath + fileName;
        if (!isWithinFSRoot(fullPath, uploadPath))
            return handleError(Forbidden);

        if (remove(fullPath.c_str()) != 0)
        {
            if (errno == ENOENT)
                return handleError(NotFound);
            return handleError(Forbidden);
        }
        HttpResponse res(ka, pr.http_v);
        res.setStatus(NoContent);
        return res;
    }
}

HttpResponse RequestHandler::serveFile()
{
    std::string body;
    if (!readFile(r.file_path, body))
        return handleError(Forbidden);
    std::string contentType = getMime(r.file_path);
    if (contentType.find("text/") == 0 || contentType == "application/javascript")
        contentType += "; charset=utf-8";

    HttpResponse res(ka, pr.http_v);
    res.setHeader("Content-Type", contentType);
    res.setHeader("Content-Length", std::to_string(body.size()));
    res.setStatus(Ok);
    res.setBody(body);
    return res;
}

/* std::string generateFilename()
{
    static size_t count = 0;
    std::ostringstream oss;
    oss << std::time(nullptr) << "_" << count++;
    return oss.str();
} */

/* HttpResponse RequestHandler::uploadFile()
{
    if (!r.location || r.location->upload_path.empty())
        return handleError(MethodNotAllowed);
    if (pr.body.empty())
        return handleError(BadRequest);

    std::string uploadPath = r.location->upload_path;
    if (uploadPath.back() != '/')
        uploadPath += '/';

    std::string fileName = generateFilename();

    if (pr.content_type.find("multipart/form-data") != std::string::npos)
        fileName += getMimeExt(pr.mp.content_type);
    else
        fileName += getExtUri(r.file_path);

    std::string fullPath = uploadPath + fileName;
    if (!isWithinFSRoot(fullPath, uploadPath))
        return handleError(Forbidden);

    std::ofstream filey(uploadPath + fileName, std::ios::binary);
    if (!filey.is_open())
        return handleError(NotFound);

    filey.write(pr.body.data(), pr.body.size()); //mao
    filey.close();

    HttpResponse res(ka, pr.http_v);
    res.setStatus(Created);
    std::string fullTargetPath = r.file_path;
    if (fullTargetPath.back() != '/')
        fullTargetPath += '/' + fileName;
    res.setHeader("Location", fullTargetPath);
    return res;
} */

HttpResponse RequestHandler::handleFiles()
{
    if (r.type == UPLOAD && pr.method == "DELETE")
        return deleteFile();
    if (!std::filesystem::exists(r.file_path))
        return handleError(NotFound);
    else
        return serveFile();
}

HttpResponse RequestHandler::handleError(int status)
{
    HttpResponse res(ka, pr.http_v);
    std::string body;
    std::map<int, std::string>::const_iterator it = sc.error_page.find(status);

    if (it != sc.error_page.end())
    {
        std::string errorPagePath = sc.root;
        if (errorPagePath.back() != '/')
            errorPagePath += '/';
        errorPagePath += it->second;

        if (isWithinFSRoot(errorPagePath, sc.root))
        {
            if (!readFile(errorPagePath, body))
                body = generateErrorPage(status, mapStatus(status));
        }
        else
            body = generateErrorPage(status, mapStatus(status));
    }
    else
        body = generateErrorPage(status, mapStatus(status));
    if (status == MethodNotAllowed)
    {
        std::string allowedM;
        if (r.location && !r.location->allow_methods.empty())
        {
            for (size_t i = 0; i < r.location->allow_methods.size(); ++i)
            {
                allowedM += r.location->allow_methods[i];
                if (i < r.location->allow_methods.size() - 1)
                    allowedM += ", ";
            }
        }
        else 
            allowedM = "GET, POST, DELETE";
        res.setHeader("Allow", allowedM);
    }

    res.setHeader("Content-Type", "text/html");
    res.setHeader("Content-Length", std::to_string(body.size()));
    res.setStatus(status);
    res.setBody(body);
    return res;
}

bool readFile(const std::string& filepath, std::string& body)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    body = buffer.str();
    return true;
}

HttpResponse RequestHandler::handleRedirect()
{
    HttpResponse res(ka, pr.http_v);
    if (r.status != 301 && r.status != 302 && r.status != 303 && r.status != 307 && r.status != 308)
        return handleError(r.status);
    res.setStatus(r.status);
    res.setHeader("Location", r.redirect_url);

    return res;
}