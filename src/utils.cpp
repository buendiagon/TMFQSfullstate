#include <random>
#include "utils.h"

// the seed for the random number engine is obtained using this
std::random_device ramdomDevice;  

// Mersenne_twister_engine using randomDev as seed
std::mt19937 randomNumberEngine(ramdomDevice()); 

// Uniform real distribuition
std::uniform_real_distribution<> uniformRealDistribution(0.0, 1.0);

double getRandomNumber() {
   return uniformRealDistribution(randomNumberEngine);
}

std::vector<double> normalizeArray(std::vector<double> inArray) {
	double norm = 0.0;

	#pragma opm parallel for
	for(unsigned int i=0; i<inArray.size(); i++) {
		inArray[i] = getRandomNumber();
	}

	#pragma omp parallel for reduction(+:sum)
	for(unsigned int i=0; i<inArray.size(); i++) {
		norm += std::pow(inArray[i], 2);
	}

	norm = std::sqrt(norm);

	#pragma opm parallel for
	for(unsigned int i=0; i<inArray.size(); i++) {
		inArray[i] = inArray[i]/norm;
	}

	return inArray;
}

// Complex number multiplication
Amplitude amplitudeMult(Amplitude a, Amplitude b){
	Amplitude result;
	result.real = a.real * b.real - a.imag * b.imag;
	result.imag = a.real * b.imag + a.imag * b.real;
	return result;
}

// Complex number addtion
Amplitude amplitudeAdd(Amplitude a, Amplitude b){
	Amplitude result;
	result.real = a.real + b.real;
	result.imag = a.imag + b.imag;
	return result;
}

// Complex number subtraction
Amplitude amplitudeSubt(Amplitude a, Amplitude b){
	Amplitude result;
	result.real = a.real - b.real;
	result.imag = a.imag - b.imag;
	return result;
}

// Convert char to int
unsigned int charToInteger(char c){
	int i;
	i = int(c) - 48;
	return i;
}


// Convert binary number (string) to decimal number (int)
unsigned int binaryToDecimal(std::string str) {
   unsigned int result = 0, i;
   for (i = 0; i < str.length(); i++) {
      result ^= (charToInteger(str[i]) << (str.length() - i - 1));
   }
   return result;
}

// Copy "numbits" bits from "source" to "destination" to the position "at"
// Example: destination = 8 (1000)
// 			source = 3 (11)
// 			at = 0
// 			numbits = 2
// 			result = 11 (1011)
unsigned int copyBits(int destination, int source, int at, int numbits){
   // int mask = ((1LL<<numbits)-1)<<at; // 1st aproach
   int mask = ((~0u)>>(sizeof(int)*8-numbits))<<at; // 2nd aproach
   return (destination&~mask)|((source<<at)&mask);
}

// e Raised to a complex number
Amplitude eRaisedToComplex(Amplitude amp){
	// e^(b+ic) = (e^b)(e^(ic))
	// e^(ic) = (cos c) + i(sin c)
	// e^(b+ic) = (e^b)((cos c) + i(sin c))
	Amplitude result;
	result.real = pow(e, amp.real) * cos(amp.imag);
	result.imag = pow(e, amp.real) * sin(amp.imag);
	return result;
}


double absoluteValue(Amplitude amp){
	return sqrt(pow(amp.real, 2) + pow(amp.imag, 2));
}

void centerText(std::string text, int nCols) {
	int nChars = text.length();
	if(nCols < nChars) {
		printf("%s", text.c_str());
	} else {
		for(int i=0; i<int((nCols-nChars)/2); i++) {
			printf(" ");
		}
		printf("%s", text.c_str());
	}
}

void repeatChar(char c, int nC) {
	for(int i=0; i<nC; i++) {
		printf("%c", c);
	}
}

void printHelp() {
	unsigned short tCol = 80;
	unsigned short nCol = 30;

	repeatChar('=', tCol);
	printf("\n");
	centerText("AYUDA DEL PROGRAMA QFT", tCol);
	printf("\n");
	repeatChar('=', tCol);
	printf("\n");
	printf("\nLos argumentos obligatorios al momento de ejecutar el programa son:\n");
	
	printf("  -nq,\033[%dG--numberQubits", 9);
	printf("\033[%dG", nCol);
	printf("Es la cantidad de qubits a generar/trabajar\n");

	printf("  -ma,\033[%dG--modeAmp", 9);
	printf("\033[%dG", nCol);
	printf("Es el modo como quiere generar las amplitudes\n;");
	printf("\033[%dG", nCol-10);
	printf("1 -> Es el modo en que genera sus propios valores\n;");
	printf("\033[%dG", nCol-10);
	printf("     de amplitudes iniciales y con estos trabaja");
	printf("\033[%dG", nCol-10);
	printf("2 -> Es el modo en que usa un archivo que tenga\n;");
	printf("\033[%dG", nCol-10);
	printf("     valores de amplitudes iniciales para usarse\n");
	printf("\033[%dG", nCol-10);
	printf("     en el simulador\n");

	printf("\n");

	printf("Dependiendo del modo se pueden ingresar otros argumentos, que serían:\n");

	printf("\n");

	printf(" MODO AMPLITUDES 1: Aleatorios propios\n");

	printf("  -r,\033[%dG--random", 9);
	printf("\033[%dG", nCol);
	printf("Se especifica si la generación de las amplitudes\n");
	printf("\033[%dG", nCol-10);
	printf("true -> Si genera los valores iniciales de manera aleatoria\n");
	printf("\033[%dG", nCol-10);
	printf("        siguiendo una distribución uniforme\n");
	printf("\033[%dG", nCol-10);
	printf("false -> No genera aleatorios para las amplitudes iniciales\n");
	printf("\033[%dG", nCol-10);
	printf("         sino toma el primer qubit en 1 y el resto en 0\n");
	printf("\033[%dG", nCol);
	printf("Aquí no es obligatorio este parámetro, se puede\n");
	printf("\033[%dG", nCol);
	printf("dejar vacío y se tomará por defecto como 'false'\n");

	printf("\n");

	printf(" MODO AMPLITUDES 2: Aleatorios externos\n");

	printf("  -eaf,\033[%dG--externalFile", 9);
	printf("\033[%dG", nCol);
	printf("Se especifica la ruta relativa o absoluta\n");
	printf("\033[%dG", nCol);
	printf("(iniciando con /) del archivo en que se encuentran\n");
	printf("\033[%dG", nCol);
	printf("las amplitudes con las cuales se va a trabajar\n");
	printf("\033[%dG", nCol);
	printf("Si no se ingresa este parámetro, toma por defecto\n");
	printf("\033[%dG", nCol);
	printf("la ruta-archivo '../intel-qs_mod/outputs/initial_states'\n");

	printf("\n");
}

unsigned int printErrors(unsigned short nError) {
	unsigned short tCol = 80;
	unsigned short nCol = 30;

	printf("\n");
	repeatChar('=', tCol);
	printf("\n");
	centerText("ERROR", tCol);
	printf("\n");
	repeatChar('=', tCol);
	printf("\n");
	printf("\n%hd -> ", nError);

	switch(nError) {
		case 1:
			printf("El número ingresado no es válido: puede tener letras o ser menor que 1\n");
		break;
		case 2:
			printf("No especificó el número de qubits sino ingresó otro parámetro\n");
		break;
		case 3:
			printf("No especificó el modo de las amplitudes\n");
		break;
		case 4:
			printf("Ingreso un número al modo de las amplitudes no válido\n");
		break;
		case 5:
			printf("El archivo seleccionado no existe o no se pudo leer\n");
		break;
	}

	return 1;
}


