/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wmardin <wmardin@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/04/07 17:49:49 by pandalaf          #+#    #+#             */
/*   Updated: 2023/08/05 21:23:04 by wmardin          ###   ########.fr       */
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
	//setBacklog(config.backlog); need this?!
	
	// Copy remaining values directly to server variables 
	_errorPagesPaths = config.getErrorPaths();
	_locations = config.getLocations();
	_cgiPaths = config.getCgiPaths();

	// Init polling structs
	_pollStructs = new pollfd[_maxConns];
	for (size_t i = 0; i < _maxConns; i++)
	{
		_pollStructs[i].fd = -1;
		_pollStructs[i].events = 0;
		_pollStructs[i].revents = 0;
	}
}

void	Server::startListening()
{
	ANNOUNCEME
	int	options = 1;
	
	_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_server_fd == -1)
		throw	socketCreationFailureException();
	
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&options, sizeof(options)) == -1)
		throw std::runtime_error(E_SOCKOPT);
	
	if (fcntl(_server_fd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error(E_FCNTL);

	if (bind(_server_fd, (struct sockaddr *) &_serverAddress, sizeof(_serverAddress)) == -1)
	{
		close(_server_fd);
		throw bindFailureException();
	}
	
	if (listen(_server_fd, SOMAXCONN) == -1)
	{
		close(_server_fd);
		throw listenFailureException();
	}

	_pollStructs[0].fd = _server_fd;
	_pollStructs[0].events = POLLIN;
	_pollStructs[0].revents = 0;

	// init tables?
}

void	Server::cleanup()
{
	if (_server_fd != -1)
		close(_server_fd);
	//pollstruct fds (client fds)
	if (_pollStructs)
		delete [] _pollStructs;
}

void	Server::poll()
{
	ANNOUNCEME
	if (::poll(_pollStructs, _clients.size() + 1, -1) == -1)
		throw pollFailureException();
}

void Server::acceptConnections()
{
	ANNOUNCEME
	if (_pollStructs[0].revents & POLLIN)
	{
		int	index = getAvailablePollStructIndex();
		// try catch block or smth to catch too many clients error in findFreeIndex
		_clients.push_back(Client(_server_fd, index)); //maybe can get rid of pollstructindex here
		Client&	newClient = _clients.back(); //CPP98?
		
		int	new_sock;
		int addrlen = sizeof(_serverAddress);
	
		new_sock = accept(_server_fd, (sockaddr*)newClient.getSockaddr(), (socklen_t*)&addrlen);
		if (new_sock == -1)
		{
			if (errno != EWOULDBLOCK) // if errno is EWOULDBLOCK that just means no more incoming connections. So not an error. But we dont wanna output a new client.
				throw std::runtime_error(E_ACCEPT);
		}
		else
		{
			_clientFDs.push_back(new_sock); //maybe not necessary but prolly yes. prolly not supposed to poll for every server.
			newClient.setClientSocketfd(new_sock);
			_pollStructs[index].fd = new_sock;
			_pollStructs[index].events = POLLIN | POLLHUP;
			std::cout << "New client accepted on fd " << new_sock << "." << std::endl;
			std::cout << "Clients size: " << _clients.size() << std::endl;
		}
	}
}

void	Server::handleConnections()
{
	ANNOUNCEME
	for (clientVec_it clientIt = _clients.begin(); clientIt != _clients.end(); ++clientIt)
	{
		std::cout << "start of loop in handleconns" << std::endl;
		if (clientIt == _clients.end())
		{
			std::cout << "how in the fuck" << std::endl;
			exit (1);
		}
		int	currentIndex = clientIt->getPollStructIndex();
		std::cout << "after getpollstructindex. pollstrucindex:" << currentIndex << std::endl;
		if (_pollStructs[currentIndex].revents & POLLIN)
			receive(clientIt);
		std::cout << "returned from receive in handleConnections. Number of clients: " << _clients.size() << std::endl;
		

	}
	std::cout << "end of handleConnectionsloop" << std::endl;
}


		/*
		was inside receive
		
		 if (clientIt->_gotRequest == true)
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



void Server::receive(clientVec_it clientIt)
{
	ANNOUNCEME
	_currentClientfd = clientIt->getSocketfd();
	
	_bytesReceived = recv(_currentClientfd, _recvBuffer, RECV_CHUNK_SIZE, 0);
	std::cout << "Received " << _bytesReceived << " bytes from client:\n\n" << _recvBuffer << std::endl;
	
	if (_bytesReceived <= 0)
	{
		closeClient(clientIt);
		std::cout << "closing because nothing received" << std::endl;
		return;
	}
	clientIt->appendToBuffer(_recvBuffer, _bytesReceived);
	clientIt->handleRequestHeader();
	if (clientIt->requestHeadComplete())
	{
		int	errorCode = 0;

		if (clientIt->_requestHead.getProtocol() != HTTPVERSION)
			errorCode = 505;
		else if (clientIt->_requestHead.getContentLength() > (int)_clientMaxBody)
			errorCode = 413;
		// check method rights at requested location
		// check file accessibility at requested location
		if (errorCode)
		{
			sendStatusCodePage(errorCode);
			closeClient(clientIt);
			return;
		}
	}
		
		
		/*
		
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
			
		} */
	
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


int Server::getAvailablePollStructIndex()
{
	ANNOUNCEME
	size_t i = 0;
	
	while (i < _maxConns && _pollStructs[i].fd != -1)
		i++;
	if (i == _maxConns)
	{
		std::cerr << __FUNCTION__ << " i = maxConns" << std::endl;
		throw connectionLimitExceededException(); // gotta catch this! this is not a kill point
		// prolly better to just retunr -1 here and check for that in calling function
		return -1;
	}
	std::cout << "pollstruct index returned: " << i <<std::endl;
	return i;
}

void Server::sendStatusCodePage(int code)
{
	Response	response(code);
		
	if (::send(_currentClientfd, response.getStatusPage(), response.getSize(), 0) == -1)
		std::cerr << "Error: Server::sendStatusCodePage: send.";
}

void Server::closeClient(clientVec_it clientIt)
{
	ANNOUNCEME
	int	currentfd = clientIt->getSocketfd();
	int	currentIndex = clientIt->getPollStructIndex();
	
	close(currentfd);
	_pollStructs[currentIndex].fd = -1;
	_pollStructs[currentIndex].events = 0;
	_pollStructs[currentIndex].revents = 0;
	std::cout << "closeClient on fd " << currentfd << std::endl;
	_clients.erase(clientIt);
	
	//_clientFDs.erase(_clientFDs);
	// Still have to remove the fd listed in clientFDs, if that list is even needed...
}

void Server::sendResponse(Response response, int socketfd)
{
	int	fileBytesSent = 0;
	int	closingBytesSent = 0;

	ANNOUNCEME
	std::cout << "Header:\n" << response._responseHeader << std::endl;
	// Send header
	if (::send(socketfd, response._responseHeader.data(), response._responseHeader.size(), 0) == -1)
		std::cerr << "Error: Server::sendResponse: send: failure to send header data.";
	std::ifstream	file;
	// Send file or corresponding status page.
	if (response._statusCode == 200)
		file.open(response.getFilePath().c_str(), std::ios::binary);
	else
		file.open(getStatusPage(response._statusCode).c_str(), std::ios::binary);
	if (file.fail())
		std::cerr << "Error: Response: send: could not open file." << std::endl;
	char	buffer[1];
	// DEBUG
	std::cout << "sending file: " << response.getFilePath() << std::endl;
	while (file.read(buffer, sizeof(buffer)))
	{
		if ((fileBytesSent += ::send(socketfd, buffer, file.gcount(), 0)) == -1)
		{
			std::cerr << "Error: Response: send: could not send file data.";
			
		}
		// DEBUG
		std::cout << "the buffer:\n" << buffer << std::endl;
	}
	// if (!file.eof())
	// {
	// 	std::cerr << "Error: Response: send: could not read entire file.";
	// 	return (-1);
	// }
	// DEBUG
	std::cout << "Reached EOF" << std::endl;
	file.close();
	// Send termination CRLFs
	std::string	terminationSequence(TERMINATION);
	if ((closingBytesSent += ::send(socketfd, terminationSequence.data(), terminationSequence.size(), 0)) == -1)
	{
		std::cerr << "Error: Response: send: failure to send termination data.";
	}
	// DEBUG
	std::cout << "Response sent, " << fileBytesSent << " bytes from file." << std::endl;
	//return (fileBytesSent);
}


void Server::whoIsI()
{
	std::cout	<< '\n'
				<< "Name(s):\t" << *_names.begin() << '\n';
					for (strVec_it it = ++_names.begin(); it != _names.end(); ++it)
						std::cout << "\t\t" << *it << '\n';
	std::cout	<< "Host:\t\t" << inet_ntoa(_serverAddress.sin_addr) << '\n'
				<< "Port:\t\t" << ntohs(_serverAddress.sin_port) << '\n'
				<< "Root:\t\t" << _root << '\n'
				<< "Dflt. dir_list:\t" << (_defaultDirListing ? "yes" : "no") << '\n'
				<< "Cl. max body:\t" << _clientMaxBody << '\n'
				<< "Max Conns:\t" << _maxConns << '\n'
				<< "Error Pages:\t" << _errorPagesPaths.begin()->first << '\t' << _errorPagesPaths.begin()->second << '\n';
					for (intStrMap_it it = ++_errorPagesPaths.begin(); it != _errorPagesPaths.end(); it++)
						std::cout << "\t\t" << it->first << '\t' << it->second << std::endl;
	std::cout	<< "Known loctns:\t" << _locations.begin()->first << '\n';
					for (strLocMap_it it = ++_locations.begin(); it != _locations.end(); it++)
						std::cout << "\t\t" << it->first << '\n';
	std::cout	<< "CGI Paths:\t" << _cgiPaths.begin()->first << '\t' << _cgiPaths.begin()->second << '\n';
					for (strMap_it it = ++_cgiPaths.begin(); it != _cgiPaths.end(); it++)
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
