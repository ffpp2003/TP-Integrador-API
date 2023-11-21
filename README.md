# TP-Integrador-API
Trabajo práctico integrador propuesto por la materia Informática 1 de la Universidad Tecnológica Nacional, Facultad Regional Córdoba.

Este trabajo práctico busca que el alumno pueda utilizar todos los conceptos aprendidos en la materia Informática 1 (Ingenieria Electronica) para realizar un código que cumpla con las siguientes tareas:

 - Comunicarse con una API de acceso público y  de tipo gratuita, para intercambiar cualquier tipo de dato.
 - Comunicarse con la API de Telegram para enviar los datos obtenidos de la API por medio de un chat privado con un usuario.

# Mí TP Integrador: API del Servicio Meteorológico Nacional
[Video de demostracion](https://youtu.be/HANMMMOLzOU).

La API que seleccione yo para mi TP integrador fue la del [Servicio Meteorologico Nacional](https://www.smn.gob.ar/). En la sección de [Datos Abiertos](https://www.smn.gob.ar/descarga-de-datos), el SMN tiene a disposición multiples "datos" a disposición del usuario.

Mi objetivo principal era poder trabajar con todas las solicitudes, sean presentes o pasadas y poder procesar todos los datos y dejarlos a disposición del usuario por via Telegram. Esto se hizo imposible por dos razones: Tiempo, y complejidad.

La particularidad del SMN es que todas las solicitudes devuelven un archivo de texto plano, cada uno con un formato distinto, pero todos respetando la delimitación de parámetros por coma (CSV). Es por eso que en mi TP, el uso de la biblioteca [cJSON](https://github.com/DaveGamble/cJSON) que recomienda el profe, no es posible para la API del SMN, por lo que para cada solicitud tendria que implementar mi propio set de funciones, o incluso crear mi propia biblioteca, para poder manejar todas los distintos datos de las distintas solicitudes. Es por eso que me termine restringiendo a solo trabajar con los datos meteorológicos presentes. 

Así y todo, procesar los datos del tiempo presente, fue un verdadero desafío, debido a la cantidad de datos que hay que procesar, y la variedad en el formato de los datos.

## Particularidades del código
Me parece importante mencionar algunas cuestiones interesantes que tiene mi codigo para que quien lea el código entienda por qué decidí hacer las cosas de la forma en la que están.

## *Uso eficiente de memoria*
En este apartado me parece importante mencionar que para la escritura del código, prioricé que el uso de la memoria sea el menor posible. Logré esto (creo), usando los distintos tipos de enteros que ofrece el archivo de cabecera `stdint.h` , para evitar asignar 4 bytes por cada variable de tipo entero (que hay muchas por todos los `for`) y poder elegir cuantos bytes quiero usar para esa variable. Ademas, trate de usar lo menos posible arreglos de variables, y decidí optar por usar arreglos dinámicos o punteros a bloques de memoria escribibles.

## *Sistema de tokenizado "extract" (casero)*
Gracias a la funcion `strtok()` de la biblioteca estandar, fue que pude extraer todos los datos de las solicitudes del SMN, y en conjunto con distintas funciones, como `atoi()`, `sscanf()` y `atof()`, fue posible convertir todos los datos a su tipo apropiado para su futuro uso.

## *Filtrado de usuarios en Telegram*
Para aumentar la robustez del código cuando se encuentra en contacto con algún usuario por telegram, el código implementa un sistema de extracción del parámetro "chatID", con el cual, una vez que recibe el mensaje de inicialización, el código solamente responde los mensajes de un solo usuario, hasta que este finaliza la sesión con el comando de salida.



# Usando el código
## Como compilar
Primero que nada es importante que creen su bot de telegram, y extraigan el token del bot y lo agreguen en la variable `telegramApiUrl` para el correcto funcionamiento.

Para compilar el código, puede ejecutar el siguiente comando:

    gcc -lcurl -Wall -lm TP10.c
  
 Es necesario tener la biblioteca de desarrollo de CURL instalada en donde compilen para poder finalizar la compilación.

## Como usar
Una vez que hayan ejecutado el programa, va a estar esperando por un nuevo mensaje de telegram en el chat privado del bot que hayan creado.
Aqui los comandos del bot:

 - **.start**: Inicia la conversación con el bot y bloquea los mensajes nuevos de otros usuarios.
 - **.list**: Lista todas las localidades disponibles para solicitar datos meteorologicos.
 - **.exit**: Finaliza la sesión con el bot para que otros usuarios puedan comenzar una conversación.
 - **.kill**: Termina la sesión con el bot y cierra el programa en la computadora.
