# Chat-OS
El repositorio incluye dos programas para el funcionamiento de un chat. Uno es para el lado del cliente (```client.c```) y otro para el servidor (```server.c```). A través de un protocolo de red. (definido en ```sistos.proto```)

## Ejecucion del Programa
### Instalación proto
Para permitir el uso del del protocolo de red con el cuál se realiza la comunicación entre el cliente/servidor es necesario realizar cualquiera de las siguiente instalaciones:

- ```
    brew install protobuf-c
- ```
    sudo apt-get update
    sudo apt-get install protobuf-c-compiler libprotobuf-c-dev
    ```

Luego, para poder compilar los archivos ```.proto``` y utilizarlos dentro del programa, ejecuta el siguiente comando:,
  
- ```
    protoc --c_out=. <nombrearchivo>.proto 
  ```
  Dode ```nombrearchivo``` es el nombre del archivo ```.proto```.

### Uso
#### Compilar el Servidor
Para compilar el servidor, ejecutar el siguiente comando:

  - ```gcc server.c sistos.pb-c.c -o server.o -lprotobuf-c```

En caso de ser necesario, reemplazar:
- ```server.c``` al nombre del archivo con el código del servidor.
- ```sistos.pb-c.c``` al nombre del archivo compilado con [proto](#instalación-proto).
- ```server.o``` por el nombre del archivo de salida al momento de compilar

Notese que se indica el uso del parámetro ```-lprotobuf-c``` de manera obligatoria.


#### Ejecutar el Servidor
  - ```<nombredelservidor> <puertodelservidor>```
  
Donde ```<nombredelservidor>``` es el nombre del programa y ```<puertodelservidor>``` es el
    número de puerto (de la máquina donde se ejecuta el servidor)

#### Compilar el Cliente
Para compilar el cliente, ejecutar el siguiente comando:

- ```gcc cliente.c sistos.pb-c.c -o client.o -lprotobuf-c```
    
En caso de ser necesario, reemplazar:
- ```cliente.c``` al nombre del archivo con el código del cliente.
- ```sistos.pb-c.c``` al nombre del archivo compilado con [proto](#instalación-proto).
- ```cliente.o``` por el nombre del archivo de salida al momento de compilar.

Notese que se indica el uso del parametro ```-lprotobuf-c``` de manera obligatoria.

#### Ejecutar el Cliente


 ``` <nombredelcliente> <nombredeusuario> <IPdelservidor> <puertodelservidor>```
    

  Donde ```<nombredelcliente>``` es el nombre del programa. ```<IPdelservidor>``` y
    ```<puertodelservidor>``` serán a donde debe llegar la solicitud de conexión del cliente según la
    configuración del servidor.

  En el cliente se permite elegir entre las siguientes opciones.
    
    1. Chatear con todos los usuarios (broadcasting).
    2. Enviar y recibir mensajes directos, privados, aparte del chat general.
    3. Cambiar de status.
    4. Listar los usuarios conectados al sistema de chat.
    5. Desplegar información de un usuario en particular.
    6. Ayuda.
    7. Salir.