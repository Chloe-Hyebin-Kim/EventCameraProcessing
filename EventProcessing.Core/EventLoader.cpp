#include "pch.h"
#include "EventLoader.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace eventcore
{
    bool EventLoader::LoadFromCsv(const std::string& filePath, std::vector<Event>& events)
    {
        ifstream file(filePath);

        if (!file.is_open())
        {
            cerr << "Failed to open file: " << filePath << endl;
            return false;
        }

        events.clear();

        std::string line;

        // header skip: t_us,x,y,p
        getline(file, line);

        while (getline(file, line))
        {
            if (line.empty())
            {
                continue;
            }

            stringstream ss(line);
            std::string token;
            Event e;

            try
            {
                getline(ss, token, ',');
                e.t_us = stoll(token);

                getline(ss, token, ',');
                e.x = stoi(token);

                getline(ss, token, ',');
                e.y = stoi(token);

                getline(ss, token, ',');
                e.polarity = stoi(token);
            }
            catch (...)
            {
                continue;
            }

            events.push_back(e);
        }

        return true;
    }
}