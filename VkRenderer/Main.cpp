#include "App.hpp"

#include <stdexcept>
#include <exception>
#include <iostream>

int main()
{
	VkRenderer::App App;

	try
	{
		App.Run();
	}
	catch (const std::exception & Ex)
	{
		std::cerr << Ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	system("PAUSE");

	return EXIT_SUCCESS;
}