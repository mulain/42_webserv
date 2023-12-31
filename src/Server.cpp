#include "../include/Server.hpp"

Server::Server(const ServerConfig& config)
{	
	_server_fd = -1;
	setHost(config.getConfigPairs()[HOST]);
	setPort(config.getConfigPairs()[PORT]);
	_configs = config.getAltConfigs();
	_configs.insert(_configs.begin(), config);
	applyHostConfig(config);	
}

void Server::applyHostConfig(const ServerConfig& config)
{
	strMap configPairs = config.getConfigPairs();
	
	// Set main values stored in configPairs
	setClientMaxBody(configPairs[CLIMAXBODY]);
	setMaxConnections(configPairs[MAXCONNS]); // not being used for now
	setDefaultDirListing(configPairs[DIRLISTING]);
	
	// Copy remaining values directly to server variables 
	_root = configPairs[ROOT];
	_standardFile = configPairs[STDFILE];
	_names = config.getNames();
	_statusPagePaths = config.getStatusPagePaths();
	_locations = config.getLocations();
	_cgiPaths = config.getCgiPaths();
	_mimeTypes = config.getMIMETypes();
}

int Server::fd()
{
	return _server_fd;
}

Server::~Server()
{
	if (_server_fd != -1)
		std::cout << "Server destructor on listening fd " << _server_fd << "." << std::endl;
}

void Server::whoIsI()
{
	std::cout	<< '\n';
				if (!_names.empty())
				{
					std::cout << "Name(s):\t" << *_names.begin() << '\n';
					for (strVec_it it = ++_names.begin(); it != _names.end(); ++it)
						std::cout << "\t\t" << *it << '\n';
				}
	std::cout	<< "Host:\t\t" << inet_ntoa(_serverAddress.sin_addr) << '\n'
				<< "Port:\t\t" << ntohs(_serverAddress.sin_port) << '\n'
				<< "Root:\t\t" << _root << '\n'
				<< "Dflt. dir_list:\t" << (_defaultDirListing ? "yes" : "no") << '\n'
				<< "Cl. max body:\t" << _clientMaxBody << '\n'
				<< "Max Conns:\t" << _maxConnections << '\n'
				<< "standardfile:\t" << _standardFile << '\n';
				if (!_statusPagePaths.empty())
				{
					std::cout << "Error Pages:\t" << _statusPagePaths.begin()->first << '\t' << _statusPagePaths.begin()->second << '\n';
					for (intStrMap_it it = ++_statusPagePaths.begin(); it != _statusPagePaths.end(); it++)
						std::cout << "\t\t" << it->first << '\t' << it->second << std::endl;
				}
				if (!_locations.empty())
				{
					std::cout	<< "Known loctns:\t" << _locations.begin()->first << '\n';
					for (strLocMap_it it = ++_locations.begin(); it != _locations.end(); it++)
						std::cout << "\t\t" << it->first << '\n';
				}
				if (!_cgiPaths.empty())
				{
					std::cout	<< "CGI Paths:\t" << _cgiPaths.begin()->first << '\t' << _cgiPaths.begin()->second << '\n';
					for (strMap_it it = ++_cgiPaths.begin(); it != _cgiPaths.end(); it++)
						std::cout << "\t\t" << it->first << '\t' << it->second << std::endl;
				}
}

void Server::startListening(std::vector<pollfd>& pollVector)
{
	ANNOUNCEME
	int		options = 1;
	pollfd	newPollStruct;
	
	_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_server_fd == -1)
		closeFdAndThrow(_server_fd);
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&options, sizeof(options)) == -1)
		closeFdAndThrow(_server_fd);;
	if (fcntl(_server_fd, F_SETFL, O_NONBLOCK) == -1)
		closeFdAndThrow(_server_fd);
	if (bind(_server_fd, (struct sockaddr*) &_serverAddress, sizeof(_serverAddress)) == -1)
		closeFdAndThrow(_server_fd);
	if (listen(_server_fd, SOMAXCONN) == -1)
		closeFdAndThrow(_server_fd);
	
	newPollStruct.fd = _server_fd;
	newPollStruct.events = POLLIN;
	newPollStruct.revents = 0;
	pollVector.push_back(newPollStruct);
	_pollVector = &pollVector;
}

/*
currently not checking for a size restriction. Revisit this during testing.
prolly just returns from accept with -1. 
if (pollVector.size() > 420)
	{
		std::cerr << I_CONNECTIONLIMIT << std::endl;
		return;
	}
*/
void Server::acceptConnections()
{
	ANNOUNCEME
	
	if (!(getPollStruct(_server_fd)->revents & POLLIN))
		return;
	while (true)
	{
		Client newClient;
		socklen_t clientAddrSize = sizeof(newClient.address);
	
		int new_sock = accept(_server_fd, (sockaddr*)&newClient.address, &clientAddrSize);
		if (new_sock == -1)
		{
			if (errno != EWOULDBLOCK)
				std::cerr << E_ACCEPT << std::endl;
			return;
		}
		if (fcntl(new_sock, F_SETFL, O_NONBLOCK) == -1)
			closeFdAndThrow(new_sock);
		newClient.fd = new_sock;
		_clients.push_back(newClient);

		pollfd new_pollStruct;
		new_pollStruct.fd = new_sock;
		new_pollStruct.events = POLLIN | POLLHUP;
		new_pollStruct.revents = 0;
		_pollVector->push_back(new_pollStruct);
	}
}

/*
this structure is meh. The state checking is too elaborate.
Wrote it before I understood that it's better (and necessary) to not 
set poll-events to everything you ever want, but only to what you want in the next step.
So the function could be separated at a higher level and doesnt have to check for the poll event /
client state so often.
It works tho.
Redo this some day. or not. 
*/
void Server::handleConnections()
{
	for (_index = 0; _index < _clients.size(); ++_index)
	{
		_clientIt = _clients.begin() + _index;
		_pollStruct = getPollStruct(_clientIt->fd);
		
		ANNOUNCEME_FD
		try
		{
			if (hangUp())
				continue;
			if (requestHead())
			{
				if (_clientIt->method == GET)
					handleGet();
				else if (_clientIt->method == POST)
					handlePost();
				else if (_clientIt->method == DELETE)
					handleDelete();
			}
			if (sendData() && responseHead())
				sendResponseBody();
		}
		catch (const std::exception& e)
		{
			std::cerr << "handleConnections catch: " << e.what() << std::endl;
		}
	}
}

bool Server::hangUp()
{
	if (_pollStruct->revents & POLLHUP)
	{
		closeClient("Server::handleConnections: POLLHUP");
		return true;
	}
	return false;
}

bool Server::receive()
{
	if (!(_pollStruct->revents & POLLIN))
		return false;
	ANNOUNCEME_FD
	char buffer[RECV_CHUNK_SIZE];
	_bytesReceived = recv(_clientIt->fd, buffer, RECV_CHUNK_SIZE, 0);
	if (_bytesReceived <= 0)
	{
		closeClient("Server::receiveData: Connection closed (no data received).");
		throw std::runtime_error(I_CLOSENODATA);
	}
	_clientIt->buffer.append(buffer, _bytesReceived);
	return true;
}

bool Server::requestHead()
{
	if (_clientIt->state > recv_head) // done receiving request head
		return true;
	ANNOUNCEME_FD
	if (!receive())
		return false;

	// basic format error: request line termination not found
	if (_clientIt->buffer.find("\r\n") == std::string::npos)
		return (sendStatusPage(400), false);
	parseRequestLine();
	
	// if buffer is empty, the request was only the request line. Not HTTP1.1 compliant, but we will accept it.
	if (!_clientIt->buffer.empty())
	{
		// check for proper headers termination
		if (_clientIt->buffer.find("\r\n\r\n") == std::string::npos)
		{
			if (_bytesReceived >= MAX_HEADERSIZE) // technically, should consider the request line here and add its size to max headersize. Also, this only makes sense if the readchunk is big enough.
				return (sendStatusPage(431), false);
			else
				return (sendStatusPage(400), false); // we read the entire "headers" but they weren't properly terminated.
		}
		parseRequestHeaders();
	}
	handleSession();
	if (requestError())
		return false;
	
	// check for body
	if (_clientIt->method != POST) // we don't process bodies of GET or DELETE requests
		_clientIt->state = handleRequest;
	else if (_clientIt->contentLength <= _clientIt->buffer.size()) // body is already complete in this recv (header content has already been deleted from buffer)
		_clientIt->state = handleRequest;
	else
		_clientIt->state = recv_body;

	selectHostConfig();
	updateClientVars();
	_clientIt->whoIsI();
	return true;
}

/*
We don't perform error checking here, because we want to log
the requests in the next step even if they generate errors.
*/
void Server::parseRequestLine()
{
	_clientIt->method = splitEraseStr(_clientIt->buffer, " ");
	_clientIt->URL = splitEraseStr(_clientIt->buffer, " ");
	_clientIt->httpProtocol = splitEraseStr(_clientIt->buffer, "\r\n");

	// check for CGI query string
	size_t questionMarkPos = _clientIt->URL.find("?");
	if (questionMarkPos != std::string::npos)
	{
		_clientIt->queryString = _clientIt->URL.substr(questionMarkPos + 1);
		_clientIt->URL = _clientIt->URL.substr(0, questionMarkPos);
	}

	// split URL for easy access
	_clientIt->URL = ifDirAppendSlash(_clientIt->URL);
	_clientIt->directory = _clientIt->URL.substr(0, _clientIt->URL.find_last_of("/") + 1);
	_clientIt->filename = _clientIt->URL.substr(_clientIt->URL.find_last_of("/") + 1);
}

void Server::handleSession()
{
	// take existing session-cookie or create new id and later transmit new session-cookie
	if (_clientIt->cookies.find(SESSIONID) != _clientIt->cookies.end())
		_clientIt->sessionId = _clientIt->cookies[SESSIONID];
	else
	{
		_clientIt->sessionId = generateSessionId();
		_clientIt->setCookie = true;
	}
	
	// write data of interest to the log
	std::string logPath = "system/logs/" + _clientIt->sessionId + ".log";
	std::ofstream logFile(logPath.c_str(), std::ios::app);
	if (logFile.is_open())
	{
		logFile << currentTime() << " " << _clientIt->method << " " << _clientIt->URL << "\n";
		logFile.close();
	}
	else
		std::cerr << "Could not open log file for tracking of session " << _clientIt->sessionId << std::endl;
	
	generateSessionLogPage(); // we are pre-generating a static page here, should do this with CGI upon request.
}

void Server::parseRequestHeaders()
{
	_clientIt->headers = parseStrMap(_clientIt->buffer, ":", "\r\n", "\r\n");
	
	// parse important headers into vars for easy access
	if (_clientIt->headers.find("host") != _clientIt->headers.end())
		_clientIt->host = _clientIt->headers["host"].substr(0, _clientIt->headers["host"].find_first_of(':')); //make a substring only up to the potential ":" to get rid of port
	
	if (_clientIt->headers.find("content-length") != _clientIt->headers.end())
		_clientIt->contentLength = atoi(_clientIt->headers["content-length"].c_str());
	
	if (_clientIt->headers.find("content-type") != _clientIt->headers.end())
		_clientIt->contentType = _clientIt->headers["content-type"];

	// the cookie header contains all cookie key-value pairs. So its return is a string map.
	if (_clientIt->headers.find("cookie") != _clientIt->headers.end())
	{
		std::string temp = _clientIt->headers["cookie"]; // parseStrMap erases from the input string. We want to preserve the cookie header to be able to pass it to CGI
		_clientIt->cookies = parseStrMap(temp, "=", ";", "Please parse me to the end!");
	}
}

void Server::handleGet()
{
	if (_clientIt->state > handleRequest)
		return;
	if (cgiRequest())
	{
		handleCGI();
		return;
	}
	if (isDirectory(_clientIt->updatedURL))
	{
		if (resourceExists(_clientIt->updatedURL + _clientIt->standardFile))
			sendFile200(_clientIt->updatedURL + _clientIt->standardFile);
		else if (_clientIt->dirListing)
		{
			generateDirListingPage(_clientIt->updatedURL);
			sendFile200(SYS_DIRLISTPAGE);
		}
		else
			sendStatusPage(404);
	}
	else
	{
		if (resourceExists(_clientIt->updatedURL))
			sendFile200(_clientIt->updatedURL);
		else
			sendStatusPage(404);
	}
}

void Server::handlePost()
{
	if (_clientIt->state > handleRequest)
		return;
	ANNOUNCEME_FD
	if (_clientIt->state == recv_body)
		receive();
	
	std::ofstream outputFile;
	std::string selectedPath;
	
	if (cgiRequest())
	{
		std::stringstream temp;
		temp << SYS_TEMPCGIIN << _clientIt->fd; // cant use to_str in cpp98
		_clientIt->path_cgiIn = temp.str();
		selectedPath = _clientIt->path_cgiIn;
	}
	else 
	{
		if (!resourceExists(_clientIt->updatedDirectory))
		{
			sendStatusPage(500);
			return;
		}
		selectedPath = _clientIt->updatedURL;
	}
	if (_clientIt->append)
		outputFile.open(selectedPath.c_str(), std::ios::binary | std::ios::app);
	else
	{
		outputFile.open(selectedPath.c_str(), std::ios::binary | std::ios::trunc);
		_clientIt->append = true;
	}
	if (!outputFile)
	{
		std::cerr << E_POSTFILE << std::endl;
		sendStatusPage(500);
		return;		
	}
	outputFile.write(_clientIt->buffer.c_str(), _clientIt->buffer.size());
	_clientIt->bytesWritten += _clientIt->buffer.size();
	_clientIt->buffer.clear();
	outputFile.close();

	if (_clientIt->bytesWritten >= _clientIt->contentLength)
	{
		if (cgiRequest())
		{
			handleCGI();
			
		}
		else
			sendEmptyStatus(201);
	}
}

void Server::handleDelete()
{
	if (_clientIt->state > handleRequest)
		return;
	if (isDirectory(_clientIt->updatedURL)) // deleting directories not allowed
	{
		sendStatusPage(405);
		return;
	}
	if (!resourceExists(_clientIt->updatedURL))
	{
		sendStatusPage(404);
		return;
	}
	if (remove(_clientIt->updatedURL.c_str()) == 0)
		sendEmptyStatus(204);
	else
		sendStatusPage(500);
}

bool Server::sendData() // this is shit. rethink entire structure now that we KNOW we have pollstatus
{
	if (!(_pollStruct->revents & POLLOUT))
		return false;
	if (_clientIt->state == recv_body)
		return false;
	return true;
}

bool Server::responseHead()
{
	if (_clientIt->state > send_head)
		return true;
	ANNOUNCEME_FD

	std::string header = buildResponseHead();
	if (send(_clientIt->fd, header.c_str(), header.size(), 0) <= 0)
	{
		closeClient("Server::sendResponseHead: send failure.");
		throw std::runtime_error(E_SEND);
	}
	std::cout << "responseHead sent to fd: " << _clientIt->fd << "\n" << header << std::endl;
	_clientIt->state = send_body;
	return false;
}

void Server::sendResponseBody()
{
	if (_clientIt->state < send_body)
		return;
	ANNOUNCEME_FD
	
	if (_clientIt->sendPath.empty())
	{
		closeClient("Server::sendResponseBody: nothing to send.");
		return;
	}
	std::ifstream fileStream(_clientIt->sendPath.c_str(), std::ios::binary);
	if (!fileStream)
	{
		fileStream.close();
		closeClient("Server::sendResponseBody: ifstream failure.");
		throw std::runtime_error("sendResponseBody: Could not open file to send. Client closed.");
	}
	char buffer[SEND_CHUNK_SIZE];
	fileStream.seekg(_clientIt->filePosition);
	fileStream.read(buffer, SEND_CHUNK_SIZE);

	if (send(_clientIt->fd, buffer, fileStream.gcount(), 0) <= 0)
	{
		fileStream.close();
		closeClient("Server::sendResponseBody: send failure.");
		throw std::runtime_error(E_SEND);
	}
	if (fileStream.eof())
	{
		fileStream.close();
		if (cgiRequest())
		{
			if (unlink(_clientIt->path_cgiOut.c_str()) != 0)
			std::cerr << E_TEMPFILEREMOVAL << std::endl;
		}
		closeClient("Server::sendResponseBody: sending complete.");
		return;
	}
	_clientIt->filePosition = fileStream.tellg();
	fileStream.close();
}

std::string Server::buildResponseHead()
{
	std::stringstream ss_header;
	size_t contentLength = fileSize(_clientIt->sendPath);
	
	ss_header	<< HTTPVERSION << ' ' << _clientIt->statusCode << ' ' << getHttpMsg(_clientIt->statusCode) << "\r\n"
				<< "Server: " << SERVERVERSION << "\r\n"
				<< "connection: close" << "\r\n"
				<< "content-length: " << contentLength << "\r\n";
	if (contentLength != 0)
		ss_header << "content-type: " << mimeType(_clientIt->sendPath) << "\r\n";
	if (_clientIt->setCookie)
		ss_header << buildCookie(SESSIONID, _clientIt->sessionId, 3600, "/") << "\r\n";
	
	ss_header	<< "\r\n";
	return ss_header.str();
}

std::string Server::ifDirAppendSlash(const std::string& path)
{
	std::string newPath = path;
	
	if (isDirectory(prependRoot(newPath)) && newPath[newPath.size() - 1] != '/')
		newPath.append("/");
	return newPath;
}

void Server::updateClientVars()
{
	// update and split URL for easy access
	// this would ideally happen in Client, but has no access to root and can't appendForwardSlash
	// move this in a future refactor
	_clientIt->URL = ifDirAppendSlash(_clientIt->URL);
	_clientIt->directory = _clientIt->URL.substr(0, _clientIt->URL.find_last_of("/") + 1);
	_clientIt->filename = _clientIt->URL.substr(_clientIt->URL.find_last_of("/") + 1);
	
	// set dir listing
	_clientIt->dirListing = dirListing(_clientIt->directory);
	
	// select the file to try to serve in case of directory
	_clientIt->standardFile = _locations[_clientIt->directory].std_file;
	if (_clientIt->standardFile.empty())
		_clientIt->standardFile = _standardFile;
	
	// check for HTTP redirection and upload redirection; if neither: updatedPath is same as URL
	std::string	http_redir = _locations[_clientIt->directory].http_redir;
	if (!http_redir.empty())
		_clientIt->updatedDirectory = http_redir;
	else if (_clientIt->method == POST && !_locations[_clientIt->directory].upload_dir.empty())
		_clientIt->updatedDirectory = _locations[_clientIt->directory].upload_dir;
	else
		_clientIt->updatedDirectory = _clientIt->directory;

	// prepend the server root if URL begins with /
	_clientIt->updatedDirectory = prependRoot(_clientIt->updatedDirectory);

	// build the new request URL
	_clientIt->updatedURL = _clientIt->updatedDirectory + _clientIt->filename;
}

void Server::sendStatusPage(int code)
{
	_clientIt->statusCode = code;
	_clientIt->state = send_head;
	_pollStruct->events = POLLOUT | POLLHUP;

	if (_statusPagePaths.find(code) == _statusPagePaths.end())
	{
		generateStatusPage(code);
		return;
	}
	std::string path = prependRoot(_statusPagePaths[code]);
	if (resourceExists(path))
		_clientIt->sendPath = path;
	else
		generateStatusPage(code);
}

void Server::sendFile200(std::string sendPath)
{
	_clientIt->statusCode = 200;
	_clientIt-> state = send_head;
	_pollStruct->events = POLLOUT | POLLHUP;
	_clientIt->sendPath = sendPath;
}

void Server::sendEmptyStatus(int code)
{
	_clientIt->statusCode = code;
	_clientIt-> state = send_head;
	_pollStruct->events = POLLOUT | POLLHUP;
	_clientIt->sendPath.clear(); // should be empty anyway, but structural symmetry
}

void Server::generateStatusPage(int code)
{
	std::ofstream errorPage(SYS_ERRPAGE, std::ios::binary | std::ios::trunc);
	if (!errorPage)
	{
		errorPage.close();
		throw std::runtime_error(E_TEMPFILE);
	}

	std::string httpMsg = getHttpMsg(code);

	errorPage	<< "<!DOCTYPE html><html><head>\n"
				<< "<title>webserv - " << code << ": " << httpMsg << "</title>\n"
				<< "<style>\n"
				<< "body {background-color: black; color: white; font-family: Arial, sans-serif; margin: 0; padding: 5% 0 0 0; text-align: center;}\n"
				<< "h1 {font-size: 42px;}\n"
				<< "p {font-size: 16px; line-height: 1.5;}\n"
				<< "</style></head>\n"
				<< "<body>\n"
				<< "<h1>" << code << ": " << httpMsg << "</h1>\n"
				<< "<img style=\"margin-left: auto;\" src=\"https://http.cat/" << code << "\" alt=\"" << httpMsg << "\">\n"
				<< "</body>\n"
				<< "</html>\n";
	errorPage.close();
	_clientIt->sendPath = SYS_ERRPAGE;
}

/*
A better way to handle this would be to not write the data from the ServerConfig to
the Server object (applyConfig()), but to just point to the currently active ServerConfig and call
its getters.
This requires a bigger refactor, because some stuff would have to be tested before the server constructor
(in ConfigFile / ServerConfig). Doable, but a bit of work. Maybe some day.
*/
void Server::selectHostConfig()
{
	if (_clientIt->host.empty())
	{
		applyHostConfig(_configs[0]); // first config is the default one
		_activeServerName = _configs[0].getNames()[0];
		return;
	}
	for (size_t i = 0; i < _configs.size(); ++i)
	{
		if (stringInVec(_clientIt->host, _configs[i].getNames()))
		{
			std::cout << "Hostname '" << _clientIt->host << "' found in ServerConfig #" << i << std::endl;
			applyHostConfig(_configs[i]);
			_activeServerName = _clientIt->host;
			return;
		}
	}
	std::cout << "Hostname '" << _clientIt->host << "' not found. Running default ServerConfig." << std::endl;
	applyHostConfig(_configs[0]);
	_activeServerName = _configs[0].getNames()[0];
}

bool Server::requestError()
{
	// wrong protocol
	if (_clientIt->httpProtocol != HTTPVERSION)
		return (sendStatusPage(505), true);
	
	// method not supported by server
	if (_clientIt->method != GET
		&& _clientIt->method != POST
		&& _clientIt->method != DELETE)
		return (sendStatusPage(501), true);

	// invalid URL for our purposes; but also so we don't have to always guard later when looking for "/"
	if (_clientIt->URL.find("/") == std::string::npos)
		return (sendStatusPage(404), true);

	// body size too large
	if (_clientIt->contentLength > _clientMaxBody)
		return (sendStatusPage(413), true); 
	
	// check requested resource
	strLocMap_it locIt = _locations.find(_clientIt->directory);
		
	// access forbidden (have to specifically allow each path in config file)
	if (locIt == _locations.end())
		return (sendStatusPage(404), true); // returning 404, not 403, to not leak file structure

	// access granted, but not for the requested method
	if ((_clientIt->method == GET && !locIt->second.get)
		|| (_clientIt->method == POST && !locIt->second.post)
		|| (_clientIt->method == DELETE && !locIt->second.delete_))
		return (sendStatusPage(405), true);
	
	return false;
}

void Server::closeClient(const char* msg)
{
	if (msg)
		std::cout << "closeClient on fd " << _clientIt->fd << ": " << msg << std::endl;
	close(_clientIt->fd);

	// erase corresponding pollStruct
	std::vector<pollfd>::iterator it = _pollVector->begin();
	
	
	
	while (it != _pollVector->end() && it->fd != _clientIt->fd)
		++it;
	if (it == _pollVector->end())
		throw std::runtime_error("Server::closeClient: fd to close not found in pollVector");
	_pollVector->erase(it);

	// erase client and decrement _index to not skip the next client in the for loop
	_clients.erase(_clientIt);
	--_index;
}

bool Server::cgiRequest()
{
	std::string extension = fileExtension(_clientIt->filename);
	if (_cgiPaths.find(extension) == _cgiPaths.end())
		return false;
	_cgiExecPath = _cgiPaths[extension];
	return true;
}

std::string Server::prependRoot(const std::string& path)
{
	if (path.find('/') == 0)
		return _root + path.substr(1);
	else
		return path;
}

pollfd* Server::getPollStruct(int fd)
{
	std::vector<pollfd>::iterator it = _pollVector->begin();
		
	while (it != _pollVector->end() && it->fd != fd)
		++it;
	if (it == _pollVector->end())
		throw std::runtime_error("Server::handleConnections: fd to handle not found in pollVector");
	return &*it;
}

std::string	Server::mimeType(const std::string& filepath)
{
	std::string extension;
	strMap_it	it;
	std::string defaultType = "application/octet-stream";

	extension = fileExtension(filepath);
	it = _mimeTypes->find(extension);
	if (it != _mimeTypes->end())
		return it->second;
	return defaultType;
}
	
bool Server::dirListing(const std::string& path)
{
	strLocMap_it	locIt =_locations.find(path);
	
	if (locIt == _locations.end())
		return false;
	if (locIt->second.dir_listing == "yes")
		return true;
	if (locIt->second.dir_listing == "no")
		return false;
	if (!_defaultDirListing)
		return false;
	return true;
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

void Server::setClientMaxBody(std::string input)
{
	if (input.find_first_not_of("0123456789") != std::string::npos)
			throw std::runtime_error(E_MAXCLIENTBODYINPUT + input + '\n');
	_clientMaxBody = atol(input.c_str());
	if (_clientMaxBody > MAX_MAXCLIENTBODY)
		throw std::runtime_error(E_MAXCLIENTBODYVAL + input + '\n');
}

void Server::setMaxConnections(std::string input)
{
	if (input.find_first_not_of("0123456789") != std::string::npos)
			throw std::runtime_error(E_MAXCONNINPUT + input + '\n');
	_maxConnections = atoi(input.c_str());
	if (_maxConnections > MAX_MAXCONNECTIONS)
		throw std::runtime_error(E_MAXCONNVAL + input + '\n');
}

void Server::setDefaultDirListing(std::string input)
{
	if (input == "yes")
		_defaultDirListing = true;
	else
		_defaultDirListing = false;
}

std::string Server::buildCookie(const std::string& key, const std::string& value, int expiration, const std::string& path)
{
	std::stringstream cookie;
	cookie << "set-cookie: " << key << "=" << value << ";";
	if (expiration >= 0)
		cookie << "max-age=" << expiration << ";";
	cookie << "path=" << path << ";";
	return cookie.str();
}

void Server::generateSessionLogPage()
{
	std::ofstream sessionLogPage(SITE_LOGPAGE, std::ios::binary | std::ios::trunc);
	if (!sessionLogPage)
	{
		sessionLogPage.close();
		throw std::runtime_error(E_TEMPFILE);
	}
	
	std::string path_LogFile = "system/logs/" + _clientIt->sessionId + ".log";
	std::ifstream logFile(path_LogFile.c_str());
	if (!logFile)
	{
		logFile.close();
		throw std::runtime_error(E_TEMPFILE);
	}

	sessionLogPage	<< "<!DOCTYPE html>\n<html>\n"
					<< "<head>\n"
					<< "<title>webserv - session log</title>\n"
					<< "<link rel=\"stylesheet\" type=\"text/css\" href=\"styles.css\"/>\n"
					<< "</head>\n"
					<< "<body>\n"
					<< "<div class=\"logContainer\">\n"
					<< "<h2>" << "Log for session id " << _clientIt->sessionId << "</h2>\n"
					<< "<logtext>" << logFile.rdbuf() << "</logtext>\n"
					<< "</div>\n"
					<< "<img style=\"margin-left: auto; position: absolute; top: 0; right: 0; height: 100%; z-index: 1;\" src=\"/img/catlockHolmes.png\">\n"
					<< "</body>\n</html>\n";
	sessionLogPage.close();
}

void Server::generateDirListingPage(const std::string& directory)
{
	std::ofstream dirListPage(SYS_DIRLISTPAGE);

	if (!dirListPage)
	{
		dirListPage.close();
		throw std::runtime_error(E_TEMPFILE);
	}

	dirListPage	<< "<head><title>Test Website for 42 Project: webserv</title><link rel=\"stylesheet\" type=\"text/css\" href=\"/styles.css\"/></head>"
				<< "<html><body><h1>Directory Listing</h1><ul>";

	struct dirent* ent;
	DIR* dir = opendir(directory.c_str());
	
	if (dir)
	{
		while ((ent = readdir(dir)) != NULL)
		{
			if (strcmp(ent->d_name, ".") == 0)
				continue;
			if (ent->d_type == DT_DIR) // append a slash if it's a directory
				dirListPage << "<li><a href=\"" << _clientIt->directory + ent->d_name << "/\">" << ent->d_name << "/</a></li>";
			else
				dirListPage << "<li><a href=\"" << _clientIt->directory + ent->d_name << "\">" << ent->d_name << "</a></li>";
		}
		closedir(dir);
	}

	dirListPage << "</ul></body></html>";
	dirListPage.close();
}

strVec Server::buildCGIenv()
{
	// prepare non insta-insertables
	std::stringstream contentLength;
	contentLength << _clientIt->contentLength; // best way I found to convert numerical to str in cpp98

	std::string cookie;
	if (_clientIt->headers.find("cookie") != _clientIt->headers.end())
		cookie = _clientIt->headers["cookie"];

	std::string ipAddress = inet_ntoa(_clientIt->address.sin_addr);

	std::stringstream port;
	port << ntohs(_serverAddress.sin_port);

	std::string userAgent;
	if (_clientIt->headers.find("user-agent") != _clientIt->headers.end())
		userAgent = _clientIt->headers["user-agent"];
	
	std::string cgiInFile = _clientIt->path_cgiIn;


	strVec env;
	env.push_back("SCRIPT_NAME=" + _clientIt->filename);
	env.push_back("QUERY_STRING=" + _clientIt->queryString);
	env.push_back("REQUEST_METHOD=" + _clientIt->method);
	env.push_back("CONTENT_TYPE=" + _clientIt->contentType);
	env.push_back("CONTENT_LENGTH=" + contentLength.str());
	env.push_back("HTTP_COOKIE=" + cookie);
	env.push_back("REMOTE_ADDR=" + ipAddress);
	env.push_back("SERVER_NAME=" + _activeServerName);
	env.push_back("SERVER_PORT=" + port.str());
	env.push_back("PATH_INFO=" + _clientIt->updatedURL);
	env.push_back("HTTP_USER_AGENT=" + userAgent);
	env.push_back("INPUT_FILE=" + _clientIt->path_cgiIn);
	env.push_back("OUTPUT_FILE=" + _clientIt->path_cgiOut);
	return env;
}

void Server::handleCGI()
{
	char* argv[3];
	argv[0] = const_cast<char*>(_cgiExecPath.c_str()), // name of executable (giving path here but hey)
	argv[1] = const_cast<char*>(_clientIt->updatedURL.c_str()), // path to script, which is the requested file
	argv[2] = NULL;
	
	std::stringstream temp;
	temp << SYS_TEMPCGIOUT << _clientIt->fd << ".html"; // cant use to_str in cpp98
	_clientIt->path_cgiOut = temp.str();

		std::cout << "path_cgiout:" << _clientIt->path_cgiOut << std::endl;

	std::ofstream cgiOutFile(_clientIt->path_cgiOut.c_str());
	if (!cgiOutFile)
	{
		cgiOutFile.close();
		sendStatusPage(500);
		return;
	}
	
	strVec envVec = buildCGIenv(); // this vector allocates for the strings
	std::vector<char*>env; // this will hold the char* to the strings and can be passed to execve
	for (size_t i = 0; i < envVec.size(); ++i)
		env.push_back(const_cast<char*>(envVec[i].c_str()));
	env.push_back(NULL);


	// create information file for child
	
	pid_t cgiPid = fork();
	if (cgiPid == -1)
	{
		std::cerr << E_FORK << std::endl;
		sendStatusPage(500);
		return;
	}
	if (cgiPid == 0)
	{
		execve(_cgiExecPath.c_str(), argv, env.data());
		std::cerr << E_EXECVE << std::endl;
		exit(EXIT_FAILURE);
	}
	else
	{
		int status;
		//waitpid(cgiPid, &status, 0); //WNOHANG?
		
		time_t childBirth = time(NULL);
		
		while (childBirth + CGI_TIMEOUT > time(NULL))
		{
			if (waitpid(cgiPid, &status, WNOHANG) != 0)
				break;
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) // WIFEXITED(status) is only true if child terminated of its own accord
		{
			std::cerr << E_CHILD << std::endl;
			sendStatusPage(500);
			return;
		}
		sendFile200(_clientIt->path_cgiOut);

		if (_clientIt->method == POST)
		{
			if (unlink(_clientIt->path_cgiIn.c_str()) != 0)
				std::cerr << E_TEMPFILEREMOVAL << std::endl;
		}

	}
}
