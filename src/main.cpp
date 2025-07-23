


// Standard includes
#include <cstdlib>
#include <iostream>

void print_debug()
{
   
}

void load()
{

    print_debug();
}

int main()
{
    try
    {
       
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}