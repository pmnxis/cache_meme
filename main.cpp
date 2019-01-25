#include "cache.h"
#include <fstream>
#include <iostream>
#include <string>
//#define FF

int example(int type) {
	int rule;
	#ifdef _MSC_VER
	std::string sim_path = \
		"C:\\msys64\\home\\pmnxis\\trace_1.din";
	std::ifstream sim_file(sim_path);
	#endif
	#ifndef _MSC_VER
	std::string sim_path = \
		"./trace_1.din";
	std::ifstream sim_file(sim_path.c_str() , std::ifstream::in);
	#endif
	
	if (!(sim_file.is_open())) {
		std::cout << "failed to open" << std::endl;
	}
	std::string line;
	Cache cData(256 * 1024, 16, 4, type);
	cData.name = "Data";
	Cache cISA(256 * 1024, 16, 4, type);
	cISA.name = "ISA ";
	long nl = 0;
	if (type == TYPE_FIFO)std::cout << "FIFO SIMULATION" << std::endl;
	if (type == TYPE_LRU)std::cout <<  "LRU  SIMULATION" << std::endl;
	while (std::getline(sim_file, line)) {
		nl++;
		cData.parseSimLine(line.c_str(), 0, 1, nl);
		cISA.parseSimLine(line.c_str(), 2, 3, nl);
	}
	sim_file.close();
	cISA.printReport();
	cData.printReport();
	return 0;
}


int main() {
	example(TYPE_FIFO);
	example(TYPE_LRU);
	std::cin.get();
}
