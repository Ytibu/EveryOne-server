#include "config_manager.h"
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <string>
#include "logger.h"

ConfigManager::ConfigManager()
{
    boost::filesystem::path configFilePath = boost::filesystem::current_path() / "config.ini";
    LOG_INFO(std::string("Loading config from: ") + configFilePath.string());
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(configFilePath.string(), pt);
    for (const auto &section : pt)
    {
        SectionInfo sectionInfo;
        for (const auto &keyValue : section.second)
        {
            sectionInfo.keyValues[keyValue.first] = keyValue.second.get_value<std::string>();
        }
        _config_map[section.first] = sectionInfo;
    }

    // 输出所有Kv对
    for (const auto &section : _config_map)
    {
        LOG_INFO(std::string("[") + section.first + "]");
        for (const auto &keyValue : section.second.keyValues)
        {
            LOG_INFO(keyValue.first + "=" + keyValue.second);
        }
    }
}

ConfigManager &ConfigManager::operator=(const ConfigManager &other)
{
    if (this != &other)
    {
        _config_map = other._config_map;
    }
    return *this;
}

SectionInfo ConfigManager::operator[](const std::string &section) const
{
    if (_config_map.find(section) == _config_map.end())
    {
        return SectionInfo();
    }
    return _config_map.at(section);
}