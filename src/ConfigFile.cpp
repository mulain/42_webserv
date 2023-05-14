/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigFile.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wmardin <wmardin@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/05/09 16:03:29 by wmardin           #+#    #+#             */
/*   Updated: 2023/05/13 22:18:44 by wmardin          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/ConfigFile.hpp"

ConfigFile::ConfigFile(const char* userConfigPath)
{
	std::string	configData;
	
	// parse default ServerConfig object from internal default config file
	configData = loadFile(PATH_DEFAULTCONFIG);
	ServerConfig	defaultConfig(getServerConfigElement(configData));
	_defaultServerConfig = defaultConfig;
	std::cout << I_DEFAULTIMPORT << PATH_DEFAULTCONFIG << std::endl;
	
	// parse intended ServerConfig objects from client supplied config file
	configData = loadFile(userConfigPath);
	while (!configData.empty())
	{
		ServerConfig newConfig(_defaultServerConfig);
		newConfig.applySettings(getServerConfigElement(configData));
		_serverConfigs.push_back(newConfig);
	}
	if (_serverConfigs.empty())
		throw std::runtime_error(E_NOSERVER);
	std::cout << I_CONFIGIMPORT << std::endl;
}

std::vector<ServerConfig> ConfigFile::getConfigs() const
{
	return _serverConfigs;
}

std::string ConfigFile::loadFile(const char* path)
{
	std::ifstream		infile(path);
	std::stringstream	buffer;
	std::string			line;
	
	if (!infile)
	{
		std::string invalidpath(path);
		throw std::runtime_error(E_FILEOPEN + invalidpath + '\n');
	}
	while (std::getline(infile, line))
	{
		if (line[line.find_first_not_of(WHITESPACE)] == '#')
			continue;
		buffer << line;
	}
	infile.close();
	line = buffer.str();
	trim(line);
	return line;
}

std::string ConfigFile::getServerConfigElement(std::string& configData)
{
	std::string	elementTitle;
	
	elementTitle = splitEraseChars(configData, "{");
	if (elementTitle != SERVER)
		throw std::runtime_error(E_ELMNTDECL + elementTitle + '\n');
	return getInstruction(configData);
}