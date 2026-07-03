#pragma once
#include <map>
#include <string>

struct SectionInfo
{
    SectionInfo() {}
    ~SectionInfo() { keyValues.clear(); }
    SectionInfo(const SectionInfo &other) : keyValues(other.keyValues) {}
    SectionInfo &operator=(const SectionInfo &other)
    {
        if (this != &other)
        {
            keyValues = other.keyValues;
        }
        return *this;
    }

    std::map<std::string, std::string> keyValues;

    std::string operator[](const std::string &key) const
    {
        if (keyValues.find(key) == keyValues.end())
        {
            return "";
        }
        return keyValues.at(key);
    }
};

class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager()
    {
        _config_map.clear();
    }
    ConfigManager(const ConfigManager &other) : _config_map(other._config_map) {}
    ConfigManager &operator=(const ConfigManager &other);

    // bool LoadConfig(const std::string &filePath);
    // std::string GetValue(const std::string &section, const std::string &key);

    SectionInfo operator[](const std::string &section) const;

private:
    std::map<std::string, SectionInfo> _config_map;
};