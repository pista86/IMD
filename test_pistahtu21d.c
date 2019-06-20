#include <stdio.h>
#include <signal.h>   
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>    

static int fd;
static int medirTyH;

void signal_handler(int sig)
{
  if (sig == SIGINT) 
  {
    close(fd);
    medirTyH = 0;
  }
}

int main(int argc, char *argv[]) 
{
    char data[3];
    uint16_t humedad;
    uint16_t temperatura;
    float humedadFloat;
    float temperaturaFloat;
    uint8_t resolucionTemperatura;

    resolucionTemperatura = (uint8_t) atoi(argv[1]);

    // Apertura de device
    fd = open("/dev/i2c-pistahtu21d", O_RDWR);

    if (fd != 0){
        medirTyH = 1;
    }

    // Registrar handler de señal
    signal(SIGINT, signal_handler);


    if (medirTyH)
    {    
        // Escritura de configuración
        if (write(fd, &resolucionTemperatura, 1) == 0)
        {
            printf("Se escribió correctamente la configuración en el sensor resolucion %d bits\n", resolucionTemperatura);
        }

        while(medirTyH)
        {
            // lectura de valores de humedad y temperatura
            if (read(fd, &data, 4) == 0)
            {
                humedad = ((data[1] << 8) + data[0]) & 0xFFFC;
                temperatura = ((data[3] << 8) + data[2]) & 0xFFFC;

                humedadFloat = ((humedad * 125.0) / 65536) - 6;
                temperaturaFloat = ((temperatura * 175.72) / 65536) - 46.85;

                printf("Humedad: %.2f %%\n", humedadFloat);
                printf("Temperatura: %.2f °C\n", temperaturaFloat);
                printf("\n");               
            }
            sleep(2);
        }
    }

    printf("Test finalizado!\n");
    return 0;
}




