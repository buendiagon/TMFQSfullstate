# TMFQS
Quantum Simulator


######################################## PROCESO ##################################################
# Se crea una rama feature en el remoto
# Se crea una rama feature en el local
# Se hace merge local a develop
# Se hace push de develop local a develop remoto
# Se hace el merge de develop a main en el remoto




#################################### CONFIGURACIÓN INICIAL ##########################################
#CREAR UN REPOSITORIO LOCAL
cd /path/
git init
git add *.cpp
git add *.h
touch LICENSE
git add LICENSE
git commit -m 'Initial project version'


#CONFIGURAR EL REPOSITORIO LOCAL
git config --global user.name "Gilberto Javier Díaz Toro"
git config --global user.email gilberto.diaz@gmail.com
git config --global core.editor vim
git config --global init.defaultBranch main
git config pull.rebase true #Cuando es verdadero, rebase las ramas en la parte superior de la rama recuperada, 
								    #en lugar de fusionar la rama predeterminada del control remoto predeterminado 
								    #cuando se ejecuta "git pull" 



#CONFIGURAR EL REPOSITORIO REMOTO
git remote add tmfqs https://github.com/diaztoro/TMFQS.git


#GENERAR EL TOKEN EN GITHUB
Click en el menú de la foto -> Settings -> Developer settings -> Personal access token -> token (classic)


#LISTAR REPOSITORIOS REMOTOS
git remote -v


#CLONAR LA RAMA DEVELOP DEL REMOTO
git clone -b develop https://github.com/diaztoro/TMFQS.git


################################ FIN DE CONFIGURACIÓN INICIAL #######################################





########################################## FLUJO DE TRABAJO #########################################

#CREAR LA RAMA FEATURE EN EL REPOSITORIO REMOTO
diaztoro/TMFQS -> Hacer click en el selector de ramas y seleccionar "develop" -> escribir el nombre de la nueva rama -> 
hacer click en la opción "create:feature from develop" 


#CREAR LA RAMA FEATURE LOCAL DESDE EL REPOSITORIO REMOTO
git checkout -b feature tmfqs/feature

#DESARROLLAR, PROGRAMAR, RÁPIDO

#HACER UN STAGING AUTOMÁTICO Y ACTUALIZAR LA RAMA CON LOS ARCHIVOS MODIFICADOS
git commit -a --no-edit -m "Update types.h with new data structures 2022-12-28"		


#SUBIR CAMBIOS DEL REPOSITORIO LOCAL AL REPOSITORIO REMOTO
git push -u tmfqs feature


#HACER EL PULL REQUEST DESDE LA RAMA FEATURE DEL REPOSITORIO REMOTO A LA RAMA DEVELOP DEL REPOSITORIO REMOTO
diaztoro/TMFQS -> Hacer click en "Pull request" -> Hacer click en "Compare and Pull request" -> 
Seleccionar destino "develop" y origen "feature" -> Hacer click en "Create pull request" -> En la opción "Assignees" 
hacer click en "Assig yourself" -> Hacer click en "Merge Pull request" -> Finalmente hacer click en "Confirm merge"


#REPETIR EL PASO ANTERIOR PARA ACTUALIZAR LA RAMA MAIN DEL REPOSITORIO REMOTO


#SINCRONIZAR LA RAMA DEVELOP LOCAL DESDE EL REPOSITORIO REMOTO
git switch develop
git pull tmfqs develop


