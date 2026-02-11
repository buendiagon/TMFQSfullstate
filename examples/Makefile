CXX = icpx
CFLAGS  = 
DEBUG_FLAGS = #-g
CXXFLAGS = -c
INCS = -I ../include -I ./ -I /usr/local/zfp/include 

LIBS += -L ../lib64 -l tmfqsfs #-L /usr/local/zfp/lib64 -lzfp -lstdc++

all: applyHadamard applyControlledPhaseShift applyControlledNot testQregQgate qft grover getSumOfProbabilities 


#LINK MAIN PROGRAMS
applyHadamard: applyHadamard.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) applyHadamard.o -o ../bin/applyHadamard

applyControlledPhaseShift: applyControlledPhaseShift.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) applyControlledPhaseShift.o -o ../bin/applyControlledPhaseShift

applyControlledNot: applyControlledNot.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) applyControlledNot.o -o ../bin/applyControlledNot

testQregQgate: testQregQgate.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) testQregQgate.o -o ../bin/testQregQgate

qft: qft.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) qft.o -o ../bin/qft

grover: grover.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) grover.o -o ../bin/grover

getSumOfProbabilities: getSumOfProbabilities.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) getSumOfProbabilities.o -o ../bin/getSumOfProbabilities



#COMPILE SOURCE MAIN PROGRAMS
applyHadamard.o: applyHadamard.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) applyHadamard.cpp

applyControlledPhaseShift.o: applyControlledPhaseShift.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) applyControlledPhaseShift.cpp

applyControlledNot.o: applyControlledNot.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) applyControlledNot.cpp

testQregQgate.o: testQregQgate.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) testQregQgate.cpp

qft.o: qft.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) qft.cpp

grover.o: grover.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) grover.cpp

getSumOfProbabilities.o: getSumOfProbabilities.cpp
	$(CXX) $(INCS) $(CXXFLAGS) $(DEBUG_FLAG) getSumOfProbabilities.cpp


#CLEAN BINARIES
clean: 
	rm -f *.o 

