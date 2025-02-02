// Used to store and output scene statistics
#pragma once

#include <iostream>
#include <iomanip>
#include <atomic>

namespace stats
{
	// Here statistics data is stored
	inline std::atomic<int> rayTriTests{ 0 };
	inline std::atomic<int> accelStructTests{ 0 };
	inline std::atomic<size_t> triCopiesCount{ 0 };
	inline std::atomic<size_t> meshCount{ 0 };
	inline std::atomic<int> acCount{ 0 };
	inline std::atomic<int> raysCasted{ 0 };

	inline void printStats()
	{
		std::cout.precision(2);
		std::cout << "Statistics:\n";
		std::cout << "Ray triangle tests:                 " << std::setw(10) 
			<< std::scientific << rayTriTests.load() << '\n';
		std::cout << "Ray acceleration structure tests:   " << std::setw(10) 
			<< std::scientific << accelStructTests.load() << '\n';
		std::cout << "Total intersection test:            " << std::setw(10) 
			<< std::scientific << (float)rayTriTests.load() + accelStructTests.load() << '\n';
		std::cout << "Total triangle copies:              " << std::setw(10) 
			<< triCopiesCount.load() << '\n';
		std::cout << "Total triangle count:               " << std::setw(10) 
			<< meshCount.load() << '\n';
		std::cout << "Acceleration structure count:       " << std::setw(10) 
			<< acCount.load() << '\n';
		std::cout << "Rays casted:                        " << std::setw(10) 
			<< raysCasted.load() << '\n';
	}
}
