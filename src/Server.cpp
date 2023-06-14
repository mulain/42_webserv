/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wmardin <wmardin@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/04/07 17:49:49 by pandalaf          #+#    #+#             */
/*   Updated: 2023/06/14 11:36:53 by wmardin          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Server.hpp"

/* Server::Server():
	_pollStructs(NULL),
	_numConns(1)
{
	(void)_numConns;

	setNames("default");
	setHost("ANY");
	setPort("50000");
	_filePaths.insert(std::make_pair(ROOT, "default/site"));_filePaths.insert(std::make_pair(PATH_DEFAULTERRPAGE, "default/error/default.html"));
	_serverParams.insert(std::make_pair(CLIMAXBODY, 1024));

	/ setRoot(config.root);
	setDir(config.dir);
	setUploadDir(config.uploadDir);
	setCgiDir(config.cgiDir);
	setDefaultErrorPage(config.defaultErrorPage);
	setErrorPages(config.errorPages);
	
	setBacklog(config.backlog);
	setMaxConnections(config.maxConnections); 
	
	_pollStructs = new pollfd[_maxConns];
	startListening();
} */

Server::Server(const ServerConfig & config):
	_pollStructs(NULL),
	_numConns(1)
{
	(void)_numConns;
	
	// Set main values stored in configPairs
	strMap		configPairs = config.getConfigPairs();
	setNames(configPairs.find(SERVERNAME)->second);
	setHost(configPairs.find(HOST)->second);
	setPort(configPairs.find(PORT)->second);
	setRoot(configPairs.find(ROOT)->second);
	setClientMaxBody(configPairs.find(CLIMAXBODY)->second);
	setMaxConnections(configPairs.find(MAXCONNS)->second);
	setDefaultDirListing(configPairs.find(DIRLISTING)->second);
	
	// Copy remaining values directly to server variables 
	_errorPagesPaths = config.getErrorPaths();
	_locations = config.getLocations();
	_cgiPaths = config.getCgiPaths();
		
	//setBacklog(config.backlog);

	_pollStructs = new pollfd[_maxConns];
	startListening();
}

Server::~Server()
{
	if (_pollStructs)
		delete [] _pollStructs;
}

void	Server::startListening()
{
	std::cout << __FUNCTION__ << std::endl;
	_pollStructs[0].fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_pollStructs[0].fd == -1)
		throw	socketCreationFailureException();
	_pollStructs[0].events = POLLIN;
	_pollStructs[0].revents = 0;
	if (fcntl(_pollStructs[0].fd, F_SETFL, O_NONBLOCK) == -1)
		throw fileDescriptorControlFailureException();
	if (bind(_pollStructs[0].fd, (struct sockaddr *) &_serverAddress, sizeof(_serverAddress)) == -1)
	{
		close(_pollStructs[0].fd);
		throw bindFailureException();
	}
	if (listen(_pollStructs[0].fd, SOMAXCONN) == -1)
	{
		close(_pollStructs[0].fd);
		throw listenFailureException();
	}
}

void	Server::poll()
{
	std::cout << __FUNCTION__ << std::endl;
	if (::poll(_pollStructs, _clients.size() + 1, -1) == -1)
		throw pollFailureException();
}

void	Server::handleConnections()
{
	std::cout << __FUNCTION__ << std::endl;
	checkNewClients();
	std::cout << "return to " << __FUNCTION__ << std::endl;
	size_t i = 0;
	for (clientVec_it clientIt = _clients.begin(); clientIt != _clients.end(); ++clientIt, ++i)
	{
		std::cout << "Client handling, i: " << i << ", Client size: " << _clients.size() << std::endl;
		if (!checkPollEvent(i, clientIt))
			continue;
		_bytesReceived = recv(_pollStructs[i + 1].fd, _recvBuffer, RECV_CHUNK_SIZE, 0);
		if (_bytesReceived <= 0)
		{
			closeClient(i, clientIt);
			continue;
		}
		clientIt->_buffer.append(_recvBuffer, _bytesReceived);
		if (!clientIt->_gotRequestHead)
		{
			if (clientIt->_buffer.find("\r\n\r\n") != std::string::npos)
			{
				buildRequestHead(clientIt);
				std::cout << "Received " << _bytesReceived << " bytes from client:\n\n" << clientIt->_buffer << std::endl;
				std::cout << "Raw path: " << clientIt->_requestHead.getPath() << std::endl;
				std::cout << "Root+raw+index.html: " << _root << clientIt->_requestHead.getPath() << "index.html" << std::endl;
				
				if (clientBodySizeError(clientIt))
					continue;
				// maybe not needed because next action will be continue anyway
			}
		}
		else //request head is complete, handle the potential request body
		{
			int	contentLength = clientIt->_requestHead.getContentLength();
			if (contentLength <= 0) //there was no request body
			{
				Response	response(clientIt->_requestHead, *this);
				response.send(clientIt->getSocketfd(), *this);
				clientIt->resetData();
			}
			else
			{
				if (clientIt->_buffer.size() < (size_t)contentLength)
					clientIt->_buffer.append(_recvBuffer, _bytesReceived); //body not complete
				else
				{
					Response	response(clientIt->_requestHead, *this);
					// handle CGI
					response.send(clientIt->getSocketfd(), *this);
					clientIt->resetData();
				}
				
			}
		}
		/* if (clientIt->_gotRequest == true)
		{
			RequestHead		request(buffer);
			
			Response	standard(request, *this);
			// standard.setStatusCode(200);
			std::cout << "Raw path: " << request.getPath() << std::endl;
			std::cout << "Pathity path path: " << _filePaths.find(ROOT)->second << request.getPath() << "index.html" << std::endl;
			// if (*(request._path.end() - 1) == '/')
			// {
			// 	// serve index (try html, htm, shtml, php), if not present, check directory listing setting to create it (or not)
			// 	standard.setFile(_filePaths.find(ROOT)->second + request._path + "index.html");
			// }
			// if (request.getFile() != "")
			// 	standard.setFile(_filePaths.find(ROOT)->second + request._path);
			std::cout << "Sending response." << std::endl;
			standard.send(_pollStructs[i + 1].fd, *this);
			clientIt->_gotRequest = false;
		} */
	}
}

// CGI handling (for php and potentially python scripts)
			// if (request.path() == ".php")
			// {
			// 	pid = fork ()
			// 	if (pid == 0)
			// 	{
			// 		// run cgi with input file as argument or piped in
			// 		// save file to temp directory
			// 	}
			// 	// attempt to serve file (html from cgi)
			// }

bool Server::checkPollEvent(size_t clientNumber, clientVec_it client)
{
	std::cout << __FUNCTION__ << std::endl;
	if (_pollStructs[clientNumber + 1].revents & POLLIN)
		return true;
	if (_pollStructs[clientNumber + 1].revents & POLLHUP)
	{
		closeClient(clientNumber, client);
		return false;
	}
	return false;
}

void Server::checkNewClients()
{
	std::cout << __FUNCTION__ << std::endl;
	if (_pollStructs[0].revents & POLLIN)
	{
		Client newClient(_pollStructs[0].fd);
		if (_clients.size() <= (size_t) _maxConns)
			_clients.push_back(newClient);
		else
			throw connectionLimitExceededException();
		std::cout << "Clients size: " << _clients.size() << std::endl;
		_pollStructs[_clients.size()].fd = newClient.getSocketfd();
		_pollStructs[_clients.size()].events = POLLIN;
	}
}

void Server::closeClient(size_t clientNumber, clientVec_it client)
{
	std::cout << __FUNCTION__ << std::endl;
	close(_pollStructs[clientNumber + 1].fd);
	_clients.erase(client);
}

void Server::buildRequestHead(clientVec_it client)
{
	std::cout << __FUNCTION__ << std::endl;
	client->_requestHead = RequestHead(client->_buffer);
	client->_gotRequestHead = true;
	client->_buffer.erase(0, client->_buffer.find("\r\n\r\n") + 4);	
}

bool Server::clientBodySizeError(clientVec_it client)
{
	std::cout << __FUNCTION__ << std::endl;
	if (client->_requestHead.getContentLength() > (int)_clientMaxBody)
	{
		Response errorResponse(413);
		errorResponse.send(client->getSocketfd(), *this);
		client->resetData();
		return true;
	}
	return false;
}

void Server::whoIsI()
{
	std::cout	<< '\n'
				<< "Name(s):\n";
					for (strVec_it it = _names.begin(); it != _names.end(); it++)
						std::cout << "\t\t" << *it << '\n';
	std::cout	<< "Host:\t\t" << inet_ntoa(_serverAddress.sin_addr) << '\n'
				<< "Port:\t\t" << ntohs(_serverAddress.sin_port) << '\n'
				<< "Root:\t\t" << _root << '\n'
				<< "Dflt. dir_list:\t" << (_defaultDirListing ? "yes" : "no") << '\n'
				<< "Cl. max body:\t" << _clientMaxBody << '\n'
				<< "Max Conns:\t" << _maxConns << '\n'
				<< "Error Pages:\n";
					for (intStrMap_it it = _errorPagesPaths.begin(); it != _errorPagesPaths.end(); it++)
						std::cout << "\t\t" << it->first << '\t' << it->second << std::endl;
	std::cout	<< "Known locations:\n";
					for (strLocMap_it it = _locations.begin(); it != _locations.end(); it++)
						std::cout << "\t\t" << it->first << std::endl;
	std::cout	<< "CGI Paths:\n";
					for (strMap_it it = _cgiPaths.begin(); it != _cgiPaths.end(); it++)
						std::cout << "\t\t" << it->first << '\t' << it->second << std::endl;		
}

void Server::setNames(std::string input)
{
	std::string name;

	while (!input.empty())
	{
		name = splitEraseTrimChars(input, WHITESPACE);
		for (std::string::const_iterator it = name.begin(); it != name.end(); it++)
			if (!isalnum(*it) && *it != '.' && *it != '_')
				throw std::runtime_error(E_SERVERNAME + name + '\n');
		_names.push_back(name);
	}
}

void Server::setHost(std::string input)
{	
	if (input == "ANY")
		_serverAddress.sin_addr.s_addr = INADDR_ANY;
	else
	{
		_serverAddress.sin_addr.s_addr = inet_addr(input.c_str());
		if (_serverAddress.sin_addr.s_addr == INADDR_NONE)
			throw std::runtime_error(E_HOSTADDRVAL + input + '\n');
	}
	_serverAddress.sin_family = AF_INET;
}

void Server::setPort(std::string input)
{
	if (input.find_first_not_of("0123456789") != std::string::npos)
		throw std::runtime_error(E_PORTINPUT + input + '\n');
	uint16_t temp = (uint16_t) atoi(input.c_str());
	if (temp > (uint16_t) 65534)
		throw std::runtime_error(E_PORTVAL + input + '\n');
	_serverAddress.sin_port = htons(temp);
}

void Server:: setRoot(std::string input)
{
	// checkMethodAccess(input);
	_root = input;
}

void Server::setDir(std::string input)
{
	// checkMethodAccess(input);
	_dir = input;
}

void Server::setUploadDir(std::string input)
{
	// checkWriteAccess(input);
	_uploadDir = input;
}

void Server::setCgiDir(std::string input)
{
	// checkExecAccess(input);
	_cgiDir = input;
}

/* void Server::setErrorPages(std::map<size_t, std::string> input)
{
	for (size_t i = 0; i < input.size(); i++)
	for (std::map<size_t, std::string>::iterator it = input.begin(); it != input.end(); it++)
		checkReadAccess(it->second);
	_errorPagesPath = input;
} */

void Server::setClientMaxBody(std::string input)
{
	if (input.find_first_not_of("0123456789") != std::string::npos)
			throw std::runtime_error(E_MAXCLIENTBODYINPUT + input + '\n');
	_clientMaxBody = atoi(input.c_str());
	if (_clientMaxBody > MAX_MAXCLIENTBODY)
		throw std::runtime_error(E_MAXCLIENTBODYVAL + input + '\n');
}

void Server::setMaxConnections(std::string input)
{
	if (input.find_first_not_of("0123456789") != std::string::npos)
			throw std::runtime_error(E_MAXCONNINPUT + input + '\n');
	_maxConns = atoi(input.c_str());
	if (_maxConns > MAX_MAXCONNECTIONS)
		throw std::runtime_error(E_MAXCONNVAL + input + '\n');
}

void Server::setBacklog(std::string input)
{
	if (input.find_first_not_of("0123456789") != std::string::npos)
			throw std::runtime_error(E_BACKLOGINPUT + input + '\n');
	_backlog = atoi(input.c_str());
	if (_backlog > MAX_BACKLOG)
		throw std::runtime_error(E_BACKLOGVAL + input + '\n');
}

void Server::setDefaultDirListing(std::string input)
{
	if (input == "yes")
		_defaultDirListing = true;
	else
		_defaultDirListing = false;
}

std::string Server::getStatusPage(int code) const
{
	if (_errorPagesPaths.find(code) != _errorPagesPaths.end())
		return _errorPagesPaths.find(code)->second;
	else
		return _errorPagesPaths.find(-1)->second;
}

std::string	Server::getRoot() const
{
	return(_filePaths.find(ROOT)->second);
}

// void Server::errorHandler(int code, int clientfd)
// {
// 	Response	response(code, *this);
// 	response.send(clientfd, *this);
// }

// PRIVATE MEMBER FUNCTIONS

/* void Server::checkMethodAccess(std::string path)
{
	if (_GET && access(path.c_str(), R_OK) != 0)
		throw std::runtime_error(E_ACC_READ + path + '\n');
	if ((_POST | _DELETE) && access(path.c_str(), W_OK) != 0)
		throw std::runtime_error(E_ACC_WRITE + path + '\n');
}

void Server::checkReadAccess(std::string path)
{
	if (access(path.c_str(), R_OK) != 0)
		throw std::runtime_error(E_ACC_READ + path + '\n');
}

void Server::checkWriteAccess(std::string path)
{
	if (access(path.c_str(), W_OK) != 0)
		throw std::runtime_error(E_ACC_WRITE + path + '\n');
}

void Server::checkExecAccess(std::string path)
{
	if (access(path.c_str(), X_OK) != 0)
		throw std::runtime_error(E_ACC_EXEC + path + '\n');
} */

const char *	Server::invalidAddressException::what() const throw()
{
	return ("invalid IP address supplied.");
}

const char *	Server::socketCreationFailureException::what() const throw()
{
	return ("error creating socket for server.");
}

const char *	Server::fileDescriptorControlFailureException::what() const throw()
{
	return ("error controlling file descriptor to non-blocking.");
}

const char *	Server::bindFailureException::what() const throw()
{
	return ("error using bind.");
}

const char *	Server::listenFailureException::what() const throw()
{
	return ("error using listen.");
}

const char *	Server::pollFailureException::what() const throw()
{
	return ("error using poll.");
}

const char *	Server::connectionLimitExceededException::what() const throw()
{
	return ("connection limit reached.");
}

const char *	Server::sendFailureException::what() const throw()
{
	return ("error sending data to client.");
}
