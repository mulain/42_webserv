#ifndef SERVER_HPP
# define SERVER_HPP

# include "webserv.hpp"

class	Server
{
	public:
		Server(const ServerConfig &);
		~Server();
		
		void	whoIsI();
		void	startListening(std::vector<pollfd>&);
		void	handleConnections();

		int		fd();
		void	addClient(int);
	
	private:
		// main handlers
		void	receiveData();
		void	handleRequestHead();
		void	handleRequestBody();
		void	selectResponseContent();
		void	selectErrorPage(int);
		void	sendResponseHead();
		void	sendResponseBody();
		void	closeClient(const char*);
		
		// utils
		std::string		buildResponseHead();
		void			generateErrorPage(int);
		pollfd*			getPollStruct(int);
		std::string		mimeType(std::string);
		void			checkRequest();
		bool			dirListing(std::string);

		// setters
		void	setNames(std::string);
		void	setHost(std::string);
		void	setPort(std::string);
		void	setClientMaxBody(std::string);
		void	setMaxConnections(std::string);
		void	setDefaultDirListing(std::string);

		int								_server_fd;
		std::vector<std::string>		_names;
		strLocMap						_locations;
		intStrMap						_errorPagesPaths;
		size_t							_clientMaxBody;
		size_t							_maxConns;
		bool							_defaultDirListing;
		strMap							_cgiPaths;
		strMap*							_mimeTypes;
		bool							_sharedNetAddr;

		sockaddr_in						_serverAddress;
		int								_clientfd;
		clientVec_it					_clientIt;
		std::vector<Client>				_clients;
		std::vector<pollfd>*			_pollVector;
		
		std::string						_root;

	class	invalidAddressException: public std::exception
	{
		const char *	what() const throw();
	};
	class	fileDescriptorControlFailureException: public std::exception
	{
		const char *	what() const throw();
	};
	class	sendFailureException: public std::exception
	{
		const char *	what() const throw();
	};
};
#endif
