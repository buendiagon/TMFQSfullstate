#ifndef UTIL_INCLUDE
#define UTIL_INCLUDE

#include <iostream>
#include <stdio.h>
#include <fstream>
#include "types.h"

// Returns a double between 0.0 and 1.0.
double getRandomNumber(); 
std::vector<double> normalizeArray(std::vector<double> inArray);
Amplitude amplitudeAdd(Amplitude a, Amplitude b);
Amplitude amplitudeSubt(Amplitude a, Amplitude b);
Amplitude amplitudeMult(Amplitude a, Amplitude b);
unsigned int charToInteger(char c);
unsigned int binaryToDecimal(std::string str);
unsigned int copyBits(int destination, int source, int at, int numbits);
Amplitude eRaisedToComplex(Amplitude amp);
double absoluteValue(Amplitude amp);

void centerText(std::string text, int nCols);
void repeatChar(char c, int nC);

void printHelp();
unsigned int printErrors(unsigned short nError);

#endif
