/*
 * COMPILATION
 * g++ testMemConsumption.cpp -I /usr/local/zfp/include -L /usr/local/zfp/lib64 -lzfp -o memoryConsumption
 *
 * EXECUTION
 * ./memoryConsumption <size> <mode>
 * size: size of the vector 2^size
 * mode: 
 *		0: std::vector
 *		1: raw::array2d
 *		2: zfp::array2
 *
 *	CHECK MEMORY CONSUMPTION
 *	ps -eo pid,%mem,cmd  | grep Consumption | grep -v exclude
 *
 */
#include <iostream>
#include <complex>
#include <stdlib.h>
#include <unistd.h>
#include <zfp.h>
#include <zfparray2.h>
#include <zfparray1.h>
#include "array2d.h"

typedef struct {
   double real, imag;
} amplitude;

using namespace std;

void print2(zfp::array2<double>::pointer p, size_t n)
{ 
	while (n--) 
		std::cout << *p++ << std::endl;
}

void printArray2d(raw::array2d r, int size){
	int i, j;
	for(i=0; i < size; i++){
		for(j=0; j < 2; j++){
			cout << r(i,j) << " ";
		}
		cout << endl;
	}
}

int main(int argc, char *argv[]){

	if(argc != 3){
		cout << "testMem <size> <mode>" << endl;
		cout << "mode: " << endl;
		cout << "0: std::vector" << endl;
		cout << "1: raw::array2d" << endl;
		cout << "2: zfp::array2" << endl;
		cout << "3: zfp::array" << endl;
		return 1;
	}
   else{
		int i, j;
		int numberOfQubits;
		char mode;
		numberOfQubits = atoi(argv[1]);
		mode = *argv[2];

		switch(mode){
			case '0' : {
				cout << "std::vector" << endl;
				std::vector<double> v(2*pow(2,numberOfQubits), 1.0);
				sleep(30);
				break;
			}
			case '1' : {
				cout << "raw::array2d" << endl;
				//raw::array2d r(pow(2,numberOfQubits), 2);
				raw::array2d r;
				r.resize(pow(2,numberOfQubits), 2);
				printArray2d(r, numberOfQubits);
				for(i=0; i < numberOfQubits; i++){
					for(j=0; j < 2; j++){
						r(i,j) = 3.0;
					}
				}
				printArray2d(r, numberOfQubits);
				sleep(30);
				break;
			}
			case '2' : {
				cout << "zfp::array2" << endl;
				zfp::array2<double> z(pow(2,numberOfQubits), 2, 1.0);
				sleep(30);
				break;
			}
		}
		return 0;
	}
}

				//cout << 2*pow(2,numberOfQubits) << " " << v.size() << " " << v.capacity() << endl;
				/*
				for (auto i: v) {
					cout << i << endl;
				}
				*/
/*
		for (raw::array2d::iterator p = v.begin(); p != v.end(); p++) {
			*p = rand();
		}
*/
/*
		for (zfp::array2<double>::iterator p = cv.begin(); p != cv.end(); p++)
			*p = rand();
		std::cout << "cv size " << cv.size() << " x " << cv.size_x() << " y " << cv.size_y() << std::endl;
		//print2(&cv[0], cv.size());
*/
