
#include "industrial_frontier/Application.hpp"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        industrial_frontier::Application application;

        if (!application.Initialize())
        {
            return 1;
        }

        application.Run();

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Fatal application error: "
                  << exception.what() << '\n';

        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal unknown application error.\n";
        return 1;
    }
}
