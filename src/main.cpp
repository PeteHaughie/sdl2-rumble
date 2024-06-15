#include <SDL.h>
#include <iostream>
#include <cstring>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const int PORT = 123456;
std::atomic<bool> running(true);
std::vector<SDL_GameController *> controllers;

void handleRumble(SDL_GameController *controller, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration)
{
    if (SDL_GameControllerRumble(controller, low_frequency_rumble, high_frequency_rumble, duration) != 0)
    {
        std::cerr << "Error: Unable to start rumble: " << SDL_GetError() << std::endl;
    }
    else
    {
        std::cout << "Rumble started: low_freq=" << low_frequency_rumble << ", high_freq=" << high_frequency_rumble << ", duration=" << duration << " ms" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        SDL_GameControllerRumble(controller, 0, 0, 0); // Stop rumble
        std::cout << "Rumble stopped" << std::endl;
    }
}

void signalHandler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received. Shutting down..." << std::endl;
    running = false;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler); // Register signal handler for SIGINT
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    int numJoysticks = SDL_NumJoysticks();
    if (numJoysticks < 1)
    {
        std::cerr << "No joysticks connected." << std::endl;
        SDL_Quit();
        return 1;
    }

    for (int i = 0; i < numJoysticks; ++i)
    {
        if (SDL_IsGameController(i))
        {
            SDL_GameController *controller = SDL_GameControllerOpen(i);
            if (controller)
            {
                controllers.push_back(controller);
                std::cout << "Controller " << i << " (" << SDL_GameControllerName(controller) << ") opened successfully." << std::endl;
            }
            else
            {
                std::cerr << "SDL_GameControllerOpen Error for controller " << i << ": " << SDL_GetError() << std::endl;
            }
        }
        else
        {
            std::cerr << "Joystick " << i << " is not a game controller." << std::endl;
        }
    }

    if (controllers.empty())
    {
        std::cerr << "No game controllers could be opened." << std::endl;
        SDL_Quit();
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Error opening socket" << std::endl;
        for (SDL_GameController *controller : controllers)
        {
            SDL_GameControllerClose(controller);
        }
        SDL_Quit();
        return 1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        std::cerr << "Error setting socket options" << std::endl;
        close(sockfd);
        for (SDL_GameController *controller : controllers)
        {
            SDL_GameControllerClose(controller);
        }
        SDL_Quit();
        return 1;
    }

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    std::memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Error on binding: " << strerror(errno) << std::endl;
        close(sockfd);
        for (SDL_GameController *controller : controllers)
        {
            SDL_GameControllerClose(controller);
        }
        SDL_Quit();
        return 1;
    }

    std::cout << "Socket binding successful on port " << PORT << std::endl;

    if (listen(sockfd, 5) < 0)
    {
        std::cerr << "Error on listen" << std::endl;
        close(sockfd);
        for (SDL_GameController *controller : controllers)
        {
            SDL_GameControllerClose(controller);
        }
        SDL_Quit();
        return 1;
    }

    std::cout << "Listening for connections..." << std::endl;
    clilen = sizeof(cli_addr);

    while (running)
    {
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        tv.tv_sec = 1; // Timeout of 1 second
        tv.tv_usec = 0;

        int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (retval == -1)
        {
            if (running)
            {
                std::cerr << "Error on select" << std::endl;
            }
            break;
        }
        else if (retval)
        {
            int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
            if (newsockfd < 0)
            {
                if (running)
                {
                    std::cerr << "Error on accept" << std::endl;
                }
                continue;
            }

            std::cout << "Accepted a new connection" << std::endl;

            char buffer[256];
            std::memset(buffer, 0, 256);
            int n = read(newsockfd, buffer, 255);
            if (n < 0)
            {
                std::cerr << "Error reading from socket" << std::endl;
                close(newsockfd);
                continue;
            }

            std::cout << "Received command: " << buffer << std::endl;

            // Parsing the command
            int controllerIndex;
            Uint16 low_freq, high_freq;
            Uint32 duration;
            if (sscanf(buffer, "%d %hu %hu %u", &controllerIndex, &low_freq, &high_freq, &duration) == 4)
            {
                if (controllerIndex >= 0 && controllerIndex < controllers.size())
                {
                    handleRumble(controllers[controllerIndex], low_freq, high_freq, duration);
                }
                else
                {
                    std::cerr << "Invalid controller index: " << controllerIndex << std::endl;
                }
            }
            else
            {
                std::cerr << "Invalid command format" << std::endl;
            }

            // Clean up the connection
            close(newsockfd);
            std::cout << "Connection closed" << std::endl;
        }
    }

    // Clean up resources before exiting
    close(sockfd);
    for (SDL_GameController *controller : controllers)
    {
        SDL_GameControllerClose(controller);
    }
    SDL_Quit();
    std::cout << "Application closed gracefully." << std::endl;
    return 0;
}
