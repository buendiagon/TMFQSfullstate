# Test CNOT gate with initial state 1x|0..0> + 0x|0..1> + ... 0x|1..1>
#
#!/bin/bash

if [ $# -ne 1 ]
then
	echo './applyControlledNot.exe <numQubits>'
	exit
fi

numQubits=$1
endIndexTargetQubit=`echo $numQubits - 2 | bc`
endIndexControlQubit=`echo $numQubits - 1 | bc`

for targetQubit in `seq 0 $endIndexTargetQubit`
do
	initIndexControlQubit=`echo $targetQubit + 1 | bc`
	for controlQubit in `seq $initIndexControlQubit $endIndexControlQubit`
	do
		echo ------------------------------------------------------
		echo $numQubits $controlQubit $targetQubit
		echo ------------------------------------------------------
		./applyControlledNot $numQubits $controlQubit $targetQubit
	done
done
