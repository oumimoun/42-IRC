#include "Server.hpp"

std::string getReason(std::vector<std::string> command)
{
    std::string result = "";
    for (size_t i = 3; i < command.size(); i++)
    {
        if (i > 3)
            result += " ";
        result += command[i];
    }
    if (!result.empty() && result[0] == ':')
        result = result.substr(1);
    return result;
}

std::map<std::string, std::vector<std::string > > parseKickCommand(std::vector<std::string> command)
{
    std::map<std::string, std::vector<std::string > > tokens;

    if (command.size() >= 3)
    {
        std::vector<std::string> channelNames = split(command[1], ',');
        std::vector<std::string> users = split(command[2], ',');

        if (channelNames.size() == users.size())
        {
            for (size_t i = 0; i < channelNames.size(); ++i)
            {
                tokens[channelNames[i]].push_back(users[i]);
            }
        }
        else if (channelNames.size() == 1 && users.size() >= 1)
        {
            for (size_t i = 0; i < users.size(); ++i)
            {
                tokens[channelNames[0]].push_back(users[i]);
            }
        }
        else if (users.size() == 1 && channelNames.size() >= 1)
        {
            for (size_t i = 0; i < channelNames.size(); ++i)
            {
                tokens[channelNames[i]].push_back(users[0]);
            }
        }
    }
    return tokens;
}

void Server::kickCommand(Client &currClient, std::string channelName, std::string nickname, std::string reason)
{
    std::map<std::string, Channel>::iterator it;
    it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendReply(currClient.getClientFd(), ERR_NOSUCHCHANNEL(currClient.getHostName(), currClient.getNickname(), channelName));
        return;
    }
    Channel &currChannel = it->second;

    if (!currChannel.isOperator(currClient.getClientFd()))
    {
        sendReply(currClient.getClientFd(), ERR_CHANOPRIVSNEEDED(currClient.getHostName(), currClient.getNickname(), channelName));
        return;
    }

    Client *targetClient = NULL;
    for (std::map<int, Client>::iterator it_client = _clients.begin(); it_client != _clients.end(); ++it_client)
    {
        if (it_client->second.getNickname() == nickname)
        {
            targetClient = &it_client->second;
            break;
        }
    }
    if (!targetClient || !targetClient->isFullyAuthenticated())
    {
        sendReply(currClient.getClientFd(), ERR_NOSUCHNICK(currClient.getHostName(), currClient.getNickname(), nickname));
        return;
    }

    if (!currChannel.isClientInChannel(targetClient->getClientFd()))
    {
        sendReply(currClient.getClientFd(), ERR_USERNOTINCHANNEL(currClient.getNickname(), nickname, channelName));
        return;
    }

    if (currChannel.removeClientFromChannel(targetClient->getClientFd()))
    {
        std::string message = RPL_KICK(currClient.getNickname(), currClient.getHostName(), currClient.getHostName(), channelName, nickname, reason);
        currChannel.broadcastMessage(message, _clients);
        if (currChannel.getClients().empty())
        {
            _channels.erase(it);
            return;
        }
        if (currChannel.getOperators().empty())
        {
            std::vector<int> &clients = currChannel.getClients();
            std::vector<int>::iterator it_target = clients.begin();

            if (it_target != clients.end())
            {
                std::map<int, Client>::iterator it_client = _clients.find(*it_target);
                if (it_client != _clients.end())
                {
                    Client &targetClient = it_client->second;
                    currChannel.addOperator(*it_target);
                    sendReply(*it_target, RPL_YOUREOPER(targetClient.getNickname()));
                }
            }
        }
    }
}

void Server::channelKick(Client &currClient, std::vector<std::string> command)
{
    if (command.size() < 3)
    {
        sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), command[0]));
        return;
    }

    std::string reason = getReason(command);

    std::map<std::string, std::vector<std::string > > tokens = parseKickCommand(command);
    if (tokens.size() == 0)
    {
        sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), command[0]));
        return;
    }

    for (std::map<std::string, std::vector<std::string > >::iterator it = tokens.begin(); it != tokens.end(); it++)
    {
        std::string channelName = it->first;
        std::vector<std::string> users = it->second;
        for (size_t i = 0; i < users.size(); i++)
        {
            std::string nickname = users[i];
            kickCommand(currClient, channelName, nickname, reason);
        }
    }
}
