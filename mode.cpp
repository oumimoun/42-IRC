#include "Server.hpp"

bool isValidMode(char c)
{
    return (c == 'i' || c == 't' || c == 'k' || c == 'o' || c == 'l');
}

int ft_atoi(std::string parameter)
{
    size_t i = 0;
    int signe = 1;
    unsigned long long total = 0;

    while (parameter[i] >= 9 && parameter[i] <= 13)
        i++;
    if (parameter[i] == '+' || parameter[i] == '-')
    {
        if (parameter[i] == '-')
            signe = -1;
        i++;
    }
    while (parameter[i] >= '0' && parameter[i] <= '9')
    {
        if (total > (9223372036854775807ULL - (parameter[i] - '0')) / 10)
            return (signe == 1 ? -1 : 0);
        total = total * 10 + (parameter[i] - '0');
        i++;
    }
    return ((int)(signe * total));
}

bool valideNumber(const std::string &number)
{
    int start = 0;

    if (number[0] == '+' || number[0] == '-')
        start = 1;

    for (size_t i = start; i < number.size(); i++)
    {
        if (number[i] < '0' || number[i] > '9')
            return false;
    }

    return true;
}

std::vector<std::string> parseModes(const std::string &modes, Client &currClient)
{
    std::vector<std::string> result;
    bool give = true;
    for (size_t i = 0; i < modes.size(); i++)
    {
        if (modes[i] == '+')
            give = true;
        else if (modes[i] == '-')
            give = false;
        else if (isValidMode(modes[i]))
            result.push_back((give ? "+" : "-") + std::string(1, modes[i]));
        else
        {
            sendReply(currClient.getClientFd(), ERR_UNKNOWNMODE(currClient.getHostName(), currClient.getNickname(), modes[i]));
            return std::vector<std::string>();
        }
    }
    return result;
}

std::vector<std::string> parseParametres(std::vector<std::string> command)
{
    std::vector<std::string> result;
    if (command.size() > 3)
        for (size_t i = 3; i < command.size(); i++)
            result.push_back(command[i]);
    return result;
}

void Server::oModeParam(Channel &currChannel, std::string parameter, std::string mode, Client &currClient)
{
    if (parameter.empty())
    {
        sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), "MODE"));
        return;
    }

    Client *targetClient = NULL;
    for (std::map<int, Client>::iterator it_client = _clients.begin(); it_client != _clients.end(); ++it_client)
    {
        if (it_client->second.getNickname() == parameter)
        {
            targetClient = &it_client->second;
            break;
        }
    }
    if (!targetClient)
    {
        sendReply(currClient.getClientFd(), ERR_NOSUCHNICK(currClient.getHostName(), currClient.getNickname(), parameter));
        return;
    }

    if (!targetClient->isFullyAuthenticated())
    {
        sendReply(currClient.getClientFd(), ERR_NOSUCHNICK(currClient.getHostName(), currClient.getNickname(), parameter));
        return;
    }

    if (!currChannel.isClientInChannel(targetClient->getClientFd()))
    {
        sendReply(currClient.getClientFd(), ERR_USERNOTINCHANNEL(currClient.getHostName(), targetClient->getNickname(), currChannel.getName()));
        return;
    }

    if (mode == "+o")
    {
        if (currChannel.isOperator(targetClient->getClientFd()))
            return;
        currChannel.addOperator(targetClient->getClientFd());
        sendReply(targetClient->getClientFd(), RPL_YOUREOPER(targetClient->getNickname()));
    }
    else if (mode == "-o")
    {
        if (!currChannel.isOperator(targetClient->getClientFd()))
            return;
        currChannel.removeOperator(targetClient->getClientFd());
    }

    std::string modeMessage = ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " " + mode + " " + parameter + "\r\n";
    currChannel.broadcastMessage(modeMessage, _clients);
}

// void iModeParam(Channel &currChannel, const std::string &mode, Client &currClient)
// {
//     if (mode == "+i")
//     {
//         currChannel.setInviteOnly(true);
//         sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " +i" + "\r\n");
//     }
//     else if (mode == "-i")
//     {
//         currChannel.setInviteOnly(false);
//         sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " -i" + "\r\n");
//     }
// }

void Server::iModeParam(Channel &currChannel, const std::string &mode, Client &currClient)
{
    if (mode == "+i")
    {
        if (currChannel.isInviteOnly()) // Avoid redundant changes
        {
            sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " 472 " + currChannel.getName() + " :Channel is already invite-only\r\n");
            return;
        }
        currChannel.setInviteOnly(true);
    }
    else if (mode == "-i")
    {
        if (!currChannel.isInviteOnly()) // Avoid redundant changes
        {
            sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " 472 " + currChannel.getName() + " :Channel is not invite-only\r\n");
            return;
        }
        currChannel.setInviteOnly(false);
    }
    else
    {
        sendReply(currClient.getClientFd(), ERR_UNKNOWNMODE(currClient.getHostName(), mode, "MODE"));
        return;
    }

    std::string modeMessage = ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " " + mode + "\r\n";
    currChannel.broadcastMessage(modeMessage, _clients); // Notify all users in the channel
}


void pluskModeParam(Channel &currChannel, const std::string &parameter, const std::string &mode, Client &currClient)
{
    if (mode == "+k")
    {
        if (parameter.empty())
        {
            sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), "MODE"));
            return;
        }
        currChannel.setKey(parameter);
        sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " +k " + parameter + "\r\n");
    }
}

void minuskModeParam(Channel &currChannel, const std::string &mode, Client &currClient)
{
    if (mode == "-k")
    {
        currChannel.setKey("");
        sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " -k" + "\r\n");
    }
}

void minuslModeParam(Channel &currChannel, const std::string &mode, Client &currClient)
{
    if (mode == "-l")
    {
        currChannel.setUserLimit(0);
        sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " -l" + "\r\n");
    }
}

void pluslModeParam(Channel &currChannel, const std::string &parameter, const std::string &mode, Client &currClient)
{
    if (mode == "+l")
    {
        if (parameter.empty())
        {
            sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), "MODE"));
            return;
        }
        if (!valideNumber(parameter))
        {
            sendReply(currClient.getClientFd(), ERR_INVALIDMODEPARAM(currClient.getNickname(), currChannel.getName(), mode));
            return;
        }

        int limit = ft_atoi(parameter);
        if (limit <= 0)
        {
            sendReply(currClient.getClientFd(), ERR_INVALIDMODEPARAM(currClient.getNickname(), currChannel.getName(), mode));
            return;
        }

        currChannel.setUserLimit(limit);
        sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " +l " + parameter + "\r\n");
    }
}

void tModeParam(Channel &currChannel, const std::string &mode, Client &currClient)
{
    if (mode == "+t")
    {
        currChannel.setTopicLock(true);
        sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " +t" + "\r\n");
    }
    else if (mode == "-t")
    {
        currChannel.setTopicLock(false);
        sendReply(currClient.getClientFd(), ":" + currClient.getPrefix() + " MODE " + currChannel.getName() + " -t" + "\r\n");
    }
    else
    {
        sendReply(currClient.getClientFd(), ERR_INVALIDMODEPARAM(currClient.getNickname(), currChannel.getName(), mode));
    }
}

void Server::channelMode(Client &currClient, std::vector<std::string> command)
{
    if (command.size() < 2)
    {
        sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), command[0]));
        return;
    }
    std::string channelName = command[1];
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (command.size() == 2 && it != _channels.end())
    {
        sendReply(currClient.getClientFd(), RPL_CREATIONTIME(currClient.getHostName(), currClient.getNickname(), channelName, it->second.getCreationDate()));
        return;
    }
    std::vector<std::string> modes;
    if (command.size() >= 3)
        modes = parseModes(command[2], currClient);
    if (modes.empty())
    {
        sendReply(currClient.getClientFd(), ERR_UMODEUNKNOWNFLAG(currClient.getNickname(), channelName, ""));
        return;
    }
    std::vector<std::string> parameters = parseParametres(command);

    if (it == _channels.end())
    {
        sendReply(currClient.getClientFd(), ERR_NOSUCHCHANNEL(currClient.getHostName(), currClient.getNickname(), channelName));
        return;
    }
    else
    {
        Channel &currChannel = it->second;

        if (currChannel.isOperator(currClient.getClientFd()) == false)
        {
            sendReply(currClient.getClientFd(), ERR_CHANOPRIVSNEEDED(currClient.getHostName(), currClient.getNickname(), channelName));
            return;
        }
        int paramCount = 0;
        for (size_t i = 0; i < modes.size(); i++)
        {
            if (modes[i] == "+o" || modes[i] == "-o")
            {
                if ((int)parameters.size() > paramCount)
                {
                    currChannel.oModeParam(currChannel, parameters[paramCount], modes[i], currClient, _hostname); //
                    paramCount++;
                }
                else
                {
                    sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), command[0]));
                    return;
                }
            }
            else if (modes[i] == "+i" || modes[i] == "-i")
            {
                iModeParam(currChannel, modes[i], currClient);
            }
            else if (modes[i] == "+k" || modes[i] == "-k")
            {
                if (modes[i] == "+k")
                {
                    if ((int)parameters.size() > paramCount)
                    {
                        pluskModeParam(currChannel, parameters[paramCount], modes[i], currClient);
                        paramCount++;
                    }
                    else
                    {
                        sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), command[0]));
                        return;
                    }
                }
                else if (modes[i] == "-k")
                    minuskModeParam(currChannel, modes[i], currClient);
            }
            else if (modes[i] == "+l" || modes[i] == "-l")
            {
                if (modes[i] == "+l")
                {
                    if ((int)parameters.size() > paramCount)
                    {
                        pluslModeParam(currChannel, parameters[paramCount], modes[i], currClient);
                        paramCount++;
                    }
                    else
                    {
                        sendReply(currClient.getClientFd(), ERR_NEEDMOREPARAMS(currClient.getNickname(), currClient.getHostName(), command[0]));
                        return;
                    }
                }
                else if (modes[i] == "-l")
                    minuslModeParam(currChannel, modes[i], currClient);
            }
            else if (modes[i] == "+t" || modes[i] == "-t")
            {
                tModeParam(currChannel, modes[i], currClient);
            }
            else
            {
                sendReply(currClient.getClientFd(), ERR_UNKNOWNMODE(currClient.getHostName(), modes[i], "MODE"));
                return;
            }
        }
    }
}
