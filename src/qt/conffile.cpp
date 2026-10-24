// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "conffile.h"

#include "util.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <set>

namespace {

std::vector<std::string> ReadLines()
{
    std::vector<std::string> vLines;
    boost::filesystem::ifstream f(GetConfigFile());
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        vLines.push_back(line);
    }
    return vLines;
}

/** Key of an active "key=value" line, or empty for comments and other lines */
std::string LineKey(const std::string& line, std::string* pstrValue = NULL)
{
    std::string s = boost::algorithm::trim_copy(line);
    if (s.empty() || s[0] == '#' || s[0] == '[')
        return "";
    size_t pos = s.find('=');
    if (pos == std::string::npos)
        return "";
    if (pstrValue) {
        *pstrValue = boost::algorithm::trim_copy(s.substr(pos + 1));
        size_t posComment = pstrValue->find(" #");
        if (posComment != std::string::npos)
            *pstrValue = boost::algorithm::trim_copy(pstrValue->substr(0, posComment));
    }
    return boost::algorithm::trim_copy(s.substr(0, pos));
}

} // anon namespace

std::vector<std::string> ConfFile::GetValues(const std::string& strKey)
{
    std::vector<std::string> vValues;
    for (const std::string& line : ReadLines()) {
        std::string strValue;
        if (LineKey(line, &strValue) == strKey)
            vValues.push_back(strValue);
    }
    return vValues;
}

bool ConfFile::SetValues(const std::map<std::string, std::vector<std::string> >& mapValues, std::string& strError)
{
    std::vector<std::string> vIn = ReadLines(), vOut;
    std::set<std::string> setWritten;
    typedef std::map<std::string, std::vector<std::string> > map_t;

    for (const std::string& line : vIn) {
        std::string strKey = LineKey(line);
        map_t::const_iterator it = mapValues.find(strKey);
        if (strKey.empty() || it == mapValues.end()) {
            vOut.push_back(line);
            continue;
        }
        if (setWritten.insert(strKey).second) {
            for (const std::string& v : it->second)
                vOut.push_back(strKey + "=" + v);
        }
    }
    for (map_t::const_iterator it = mapValues.begin(); it != mapValues.end(); ++it) {
        if (setWritten.count(it->first))
            continue;
        for (const std::string& v : it->second)
            vOut.push_back(it->first + "=" + v);
    }

    boost::filesystem::path path = GetConfigFile();
    boost::filesystem::path pathTmp = path;
    pathTmp += ".new";
    {
        boost::filesystem::ofstream f(pathTmp, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!f.good()) {
            strError = "cannot write " + pathTmp.string();
            return false;
        }
#ifdef WIN32
        const char* eol = "\r\n";
#else
        const char* eol = "\n";
#endif
        for (const std::string& line : vOut)
            f << line << eol;
        f.close();
        if (f.fail()) {
            strError = "cannot write " + pathTmp.string();
            return false;
        }
    }
    try {
        boost::filesystem::rename(pathTmp, path);
    } catch (const boost::filesystem::filesystem_error& e) {
        strError = e.what();
        return false;
    }
    return true;
}
