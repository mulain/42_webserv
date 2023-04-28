/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wmardin <wmardin@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/26 01:51:20 by pandalaf          #+#    #+#             */
/*   Updated: 2023/04/28 16:53:05 by wmardin          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
# define WEBSERV_HPP

# include <iostream>
# include <vector>
# include <map>

// SIZE DEFINITIONS
# define RECEIVE_BUFFER			8192
# define MAX_MAXCLIENTBODY		10000
# define MAX_MAXCONNECTIONS		1000
# define MAX_BACKLOG			100

// CONFIGURATION ELEMENT IDENTIFIERS
# define SERVERNAME	"server_name"
# define HOST		"host"
# define PORT		"listen"
# define GET		"GET"
# define POST		"POST"
# define DELETE		"DELETE"
# define DIRLISTING	"directoryListing"
# define ROOT		"root"
# define DIR		"dir"
# define UPLOADDIR	"uploadDir"
# define CGIDIR		"CGIdir"
# define ERRORPAGE	"errorPage"
# define CLIMAXBODY	"clientMaxBodySize"
# define MAXCONNS	"maxConnections"
# define BACKLOG	"backlog"

// ERROR MESSAGES
// ServerConfig
# define E_FILEOPEN		"Error: ServerConfig: Could not open config file: "
# define E_NOSERVER		"Error: ServerConfig: No valid server configs found."
# define E_ELMNTDECL	"Error: ServerConfig: Invalid element declaration, (only \"server\" allowed): "
# define E_SUBELEMNT	"Error: ServerConfig: Subelements not allowed: "

// Server
# define E_SERVERNAME			"Error: Server: Invalid characters in server name input. Only alphanumerical, <<.>> and <<_>> allowed: "
# define E_HOSTADDRINPUT		"Error: Server: Invalid characters in host address input. Only numerical and dot allowed: "
# define E_HOSTADDRVAL			"Error: Server: Invalid address value. Cannot convert to IP address: "
# define E_PORTINPUT			"Error: Server: Invalid characters in port input. Only numerical allowed: "
# define E_PORTVAL				"Error: Server: Invalid port number. Must be from 0 to 65535: "
# define E_MAXCLIENTBODYINPUT	"Error: Server: Invalid characters in client max body size input. Only numerical allowed: "
# define E_MAXCLIENTBODYVAL		"Error: Server: Invalid client max body size: "
# define E_MAXCONNINPUT			"Error: Server: Invalid characters in max connections input. Only numerical allowed: "
# define E_MAXCONNVAL			"Error: Server: Invalid size of max connections: "
# define E_BACKLOGINPUT			"Error: Server: Invalid characters in backlog input. Only numerical allowed: "
# define E_BACKLOGVAL			"Error: Server: Invalid size of back log: "
# define E_ACC_READ				"Error: Server: Cannot read from path: "
# define E_ACC_WRITE			"Error: Server: Cannot write in path: "

// TYPEDEFS
typedef std::map<std::string, std::string>::const_iterator	StringMap_it;
typedef std::map<std::string, std::string> 					StringMap;

// GLOBAL FUNCTIONS
bool 			isAlnumString(const std::string&);
std::string		trim(std::string& input);
std::string		splitEraseStr(std::string&, std::string);
StringMap		splitEraseStrMap(std::string&, std::string, std::string, char);

#endif
