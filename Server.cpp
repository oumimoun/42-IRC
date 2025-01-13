#include "Server.hpp"

#include <sstream> // TODO might be removed !

Server::Server(int port, std::string password) : _server_fd(-1), _port(port), _password(password), _client_count(1) {}

void Server::run()
{
    startServer();
    while (true)
    {
        int poll_count = poll(fds, _client_count, -1);
        if (poll_count < 0)
            throw std::runtime_error("Poll failed");

        // std::cerr << "Debugg\n";
        if (fds[0].revents & POLLIN)
            handleNewClient();

        for (int i = 1; i < _client_count; i++)
        {
            if (fds[i].revents & POLLIN)
                handleClientRequest(fds[i].fd);
        }
    }
}

void Server::startServer()
{
    struct sockaddr_in server_addr;
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0)
        throw std::runtime_error("Failed to open socket");

    NonBlockingSocket server_socket(_server_fd);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(_port);

    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("Failed to set socket option SO_REUSEADDR");
    if (bind(_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        throw std::runtime_error("Failed to bind the socket");
    if (listen(_server_fd, 10) < 0)
        throw std::runtime_error("Failed to listen on socket");

    std::cout << "Server is listening on port " << _port << std::endl;

    fds[0].fd = _server_fd;
    fds[0].events = POLLIN;
}

void Server::handleNewClient()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
        throw std::runtime_error("Failed to accept client");

    NonBlockingSocket client_socket(client_fd);
    fds[_client_count].fd = client_fd;
    fds[_client_count].events = POLLIN;

    // TODO might be removed !
    std::ostringstream client_id;
    client_id << client_fd;
    _clients[client_fd] = "client " + client_id.str();

    _client_count++;
    std::cout << "New client connected!" << std::endl;
}


void Server::handleClientRequest(int client_fd)
{
    char buffer[1024];
    int bytes_read = recv(client_fd, buffer, 1024, 0);

    if (bytes_read == 0)
    {
        std::cout << "Client disconnected." << std::endl;
        removeClient(client_fd);
        return;
    }
    else if (bytes_read < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            std::cout << "another connection from same terminal" << std::endl;
        else
        {
            removeClient(client_fd);
            throw std::runtime_error("Error receiving data from client");
        }
    }
    else
    {
      buffer[bytes_read] = '\0';
    std::string message(buffer);

    std::cout << "Received: " << message;

    // Handle commands
    if (message.substr(0, 4) == "PASS")
        std::cout << "PASSWORD hhh" << std::endl;
    else if (message.substr(0, 4) == "NICK")
        std::cout << "Nick mok" << std::endl;
    else if (message.substr(0, 4) == "USER")
        std::cout << "User Flan flani : " << _clients[client_fd].getNickname() << std::endl;
    // if () // TODO iser is authentificated
    else if (message.substr(0, 4) == "JOIN")
        ChannelJoin(client_fd, message);
    else if (message.substr(0, 4) == "MODE") // TODO
        channelMode(client_fd, message);
    else if (message.substr(0, 4) == "KICK")
        channelKick(client_fd, message); // TODO to be tested when NICK is implemented
    else if (message.substr(0, 5) == "TOPIC")
        channelTopic(client_fd, message);
    else if (message.substr(0, 6) == "INVITE") // TODO
        channelInvite(client_fd, message);
    }
}

void Server::channelInvite(int client_fd, std::string message)
{
    std::string newMessage = trimString(message);
    std::vector<std::string> splits = split(newMessage, ' ');
    std::string nickname = splits[1];
    std::string channelName = splits[2];
    if (splits.size() != 3)
    {
        std::cout << "Error: INVITE <nickname> <channel>" << std::endl;
        return;
    }

    std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    if (it == _channels.end())
        std::cout << "Error: channel " << channelName << " does not exist" << std::endl;
    else
    {
        Channel currChannel = it->second;
        if (currChannel.getInviteOnly() == false)
        {
            std::cout << "Error: channel " << channelName << " is not invite only" << std::endl;
            return;
        }
        Client currClient = _clients[client_fd];
        if (currChannel.isOperator(currClient.getNickname()) == false)
        {
            std::cout << "Error: " << currClient.getNickname() << " is not an operator in channel " << channelName << std::endl;
            return;
        }
        std::map<std::string, Client>::iterator it_client = currChannel.getClients().find(nickname);
        if (it_client != currChannel.getClients().end())
        {
            std::cout << "Error: " << nickname << " is not in channel " << channelName << std::endl;
            return;
        }

        for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        {
            if (it->second.getNickname() == nickname)
            {
                if (currChannel.isInvited(nickname))
                {
                    std::cout << "Error: " << nickname << " is already invited to channel " << channelName << std::endl;
                    return;
                }
                currChannel.addInvited(nickname);
            }
        }
        
    }
}

void Server::channelMode(int client_fd, std::string message) // TODO to perform a MODE command you need to be an operator
{
    std::string newMessage = trimString(message);
    std::vector<std::string> splits = split(newMessage, ' ');
    std::string channelName = splits[1];
    std::string mode = splits[2];
    std::string parameter = splits[3];
    if (splits.size() > 4)
    {
        std::cout << "Error: invalid mode command" << std::endl;
        return;
    }

    std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    if (it == _channels.end())
    {
        std::cout << "Error: channel " << channelName << " does not exist" << std::endl;
        return;
    }
    else
    {
        Client currClient = _clients[client_fd];
        Channel currChannel = it->second;
        if (currChannel.isOperator(currClient.getNickname()) == false)
        {
            std::cout << "Error: " << currClient.getNickname() << " is not an operator in channel " << channelName << std::endl;
            return;
        }
        if (mode == "+o")
        {
            // user need to be in the channel and need to be an operator
            std::map<std::string, Client>::iterator it_client = currChannel.getClients().find(parameter);
            if (it_client == currChannel.getClients().end())
            {
                std::cout << "Error: " << parameter << " is not in channel " << channelName << std::endl;
                return;
            }
            if (currChannel.isOperator(parameter))
            {
                std::cout << "Error: " << parameter << " is already an operator in channel " << channelName << std::endl;
                return;
            }
            currChannel.addOperator(parameter);
        }
        else if (mode == "-o")
        {
            // user need to be in the channel and need to be an operator
            std::map<std::string, Client>::iterator it_client = currChannel.getClients().find(parameter);
            if (it_client == currChannel.getClients().end())
            {
                std::cout << "Error: " << parameter << " is not in channel " << channelName << std::endl;
                return;
            }
            currChannel.removeOperator(parameter);
        }
        else if (mode == "+i")
        {
            if (splits.size() != 3)
            {
                std::cout << "Error: invalid mode command try MODE <channel> {+i|-i}" << std::endl;
            }
            currChannel.setInviteOnly(true);
        }
        else if (mode == "-i")
        {
            if (splits.size() != 3)
            {
                std::cout << "Error: invalid mode command try MODE <channel> {+i|-i}" << std::endl;
            }
            currChannel.setInviteOnly(false);
        }
        else if (mode == "+k")
        {
            if (splits.size() != 4)
            {
                std::cout << "Error: invalid mode command try MODE <channel> {+k|-k} <key>" << std::endl;
            }
            currChannel.setKey(parameter);
        }
        else if (mode == "-k")
        {
            if (splits.size() != 3)
            {
                std::cout << "Error: invalid mode command try MODE <channel> {+k|-k} <key>" << std::endl;
            }
            currChannel.setKey("");
        }
        else if (mode == "+l")
        {
            if (splits.size() != 4)
            {
                std::cout << "Error: invalid mode command try MODE <channel> {+l|-l} <limit>" << std::endl;
            }
            // TODO parce the parameter
            currChannel.setUserLimit(std::stoi(parameter));
        }
        else if (mode == "-l")
        {
            if (splits.size() != 3)
            {
                std::cout << "Error: invalid mode command try MODE <channel> {+l|-l} <limit>" << std::endl;
            }
            currChannel.setUserLimit(0);
        }
        else if (mode == "+t")
        {
            if (splits.size() != 3)
            {
                std::cout << "Error: MODE <channel> {+t|-t}" << std::endl;
            }
            currChannel.setTopicLock(true);
        }
        else if (mode == "-t")
        {
            if (splits.size() != 3)
            {
                std::cout << "Error: MODE <channel> {+t|-t}" << std::endl;
            }
            currChannel.setTopicLock(false);
        }
        else
        {
            std::cout << "Error: invalid mode" << std::endl;
            return;
        }

    }
}

void Server::channelTopic(int client_fd, std::string message)
{
    std::string newMessage = trimString(message);
    std::vector<std::string> splits = split(newMessage, ' ');
    std::string channelName = splits[1];
    std::string newTopic = splits[2];
    if (splits.size() < 2 || splits.size() > 3)
    {
        std::cout << "Error: TOPIC <channel> [<topic>]\n";
        return;
    }
    std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    if (it == _channels.end())
    {
        std::cout << "Error: channel " << channelName << " does not exist" << std::endl;
        return;
    }
    else
    {
        Channel currChannel = it->second;
        Client currClient = _clients[client_fd];
        if (currChannel.isOperator(currClient.getNickname()) == false)
        {
            std::cout << "Error: " << currClient.getNickname() << " is not an operator in channel " << channelName << std::endl;
            return;
        }
        if (splits.size() == 2)
        {
            std::cout << "Topic for channel " << channelName << " is " << currChannel.getTopic() << std::endl;
        }
        else
        {
            currChannel.setTopic(newTopic);
            std::cout << "Topic for channel " << channelName << " is set to " << newTopic << std::endl;
        }
    }

}

void Server::removeClient(int client_fd)
{
    _clients.erase(client_fd);

    for (int i = 1; i < _client_count; i++)
    {
        if (fds[i].fd == client_fd)
        {
            fds[i] = fds[_client_count - 1];
            fds[_client_count - 1].fd = -1;
            _client_count--;
            return;
        }
    }
}

void Server::joinCommand(std::string channelName, std::string key, int client_fd)
{
	std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    Client currClient = _clients[client_fd];
    if (it == _channels.end())
    {
        Channel newChannel(channelName, key);
        newChannel.addClient(currClient);
        newChannel.addOperator(currClient.getNickname());
        _channels[channelName] = newChannel;
        std::cout << "Channel " << channelName << " created" << std::endl;
    }
    else
    {
        std::cout << "Channel " << channelName << " already exists" << std::endl;
        Channel currChannel = it->second;
        if (currChannel.getKey() == key)
            std::cout << "Key is correct" << std::endl;
        else
        {
            std::cout << "Key is incorrect" << std::endl;
            return;
        }
        // if (currChannel.isInviteOnly()) // TODO
        if (currChannel.getUserLimit() != 0 && currChannel.getUserCount() >= currChannel.getUserLimit())
        {
            std::cout << "Error: channel " << channelName << " is full" << std::endl;
            return;
        }
        std::map<std::string, Client>::iterator it_client = currChannel.getClients().find(currClient.getNickname());
        if (it_client == currChannel.getClients().end())
        {
            currChannel.addClient(currClient);
            std::cout << "Client " << currClient.getNickname() << " added to channel " << channelName << std::endl;
        }
        else
        {
            std::cout << "Client " << currClient.getNickname() << " already exists in channel " << channelName << std::endl;
        }
    }
}


void Server::ChannelJoin(int client_fd, std::string message)
{
    std::string trimedMessage = trimString(message);
    std::cout << "Trimed message: " << trimedMessage << std::endl;
    std::string newMessage = trimedMessage.substr(4);
	std::map<std::string, std::string> tokens = parseJoinCommand(newMessage);
    if (tokens.size() == 0)
    {
        std::cout << "Error: invalid JOIN command" << std::endl;
        return;
    }
	std::map<std::string, std::string>::iterator it;
	for (it = tokens.begin(); it != tokens.end(); it++)
	{
		joinCommand(it->first, it->second, client_fd);
	}
}

void Server::channelKick(int client_fd, std::string message)
{
    std::string newMessage = trimString(message);
    std::vector<std::string> splits = split(newMessage, ' ');
    std::string channelName = splits[1];
    std::string nickname = splits[2];
    std::string reason = splits[3]; // TODO reason is more then one word
    if (splits.size() < 3)
    {
        std::cout << "Error: KICK <channel> <nickname>" << std::endl;
        return;
    }

    std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    if (it == _channels.end())
    {
        std::cout << "Error: channel " << channelName << " does not exist" << std::endl;
        return;
    }
    else
    {
        Channel currChannel = it->second;
        Client currClient = _clients[client_fd];
        if (currChannel.isOperator(currClient.getNickname()) == false)
        {
            std::cout << "Error: " << currClient.getNickname() << " is not an operator in channel " << channelName << std::endl;
            return;
        }

        if (currChannel.removeClient(nickname))
        {
            std::cout << "Client " << nickname << " removed from channel " << channelName << std::endl;
        }

    }
    
}
