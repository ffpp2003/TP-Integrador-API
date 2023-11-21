#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <iconv.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>

typedef struct {
    char *string;
    size_t size;
} string_buffer_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
} api_date_t;

typedef struct {
    size_t tiepreLocations;
    char *location;
    size_t locStrSize;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    char *status;
    size_t stsStrSize;
    uint8_t visibility;
    float temperature;
    float temperatureST;
    uint8_t relativeHumidity;
    char *windDirection;
    size_t windDirStrSize;
    uint8_t windSpeed;
    float atmosphericPressure;
} tiepre_location_t;

char *meses[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};

void init_buff(string_buffer_t *);
void free_buff(string_buffer_t *);
void clear_buff(string_buffer_t *buff);
size_t write_callback(void *, size_t, size_t, void *);
CURL *curl_init_date_api(char *, string_buffer_t *);
CURL *curl_init_smn_api(char *, string_buffer_t *);
CURL *curl_init_telegram_api(char *, string_buffer_t *);
void request_date(CURL *, char *, string_buffer_t *, api_date_t *);
void request_smn(char *, char *, CURL *, string_buffer_t *);
void request_telegram(char *, char *, CURL *, string_buffer_t *);
char *wait_new_message_telegram(uint64_t, char *, char *, CURL *, string_buffer_t *);
char *update_update_id_method_telegram(char *, char *);
void update_id_telegram(char *, string_buffer_t *);
void wait_for_start_telegram(uint64_t *, char *, char *, char *, CURL *, string_buffer_t *);
uint64_t extract_chat_id_telegram(char *);
void extract_telegram_message(char **);
void send_message_telegram(uint64_t, char *, char *, CURL *, string_buffer_t *);
uint8_t count_digits(uint32_t n);
char *format_message_url(char *);
uint16_t get_location_cuantity_tiempre(string_buffer_t *);
char *iconvISO2UTF8(char *);
tiepre_location_t *tiepre_request(char *, CURL *, string_buffer_t *, api_date_t *);
tiepre_location_t *catalog_tiepre_request(string_buffer_t *);
void extract_location_tiempre(char *, tiepre_location_t *);
void extract_date_tiempre(char *, tiepre_location_t *);
void extract_time_tiempre(char *, tiepre_location_t *);
void extract_status_tiempre(char *, tiepre_location_t *);
void extract_visibility_tiempre(char *, tiepre_location_t *);
void extract_temperature_tiempre(char *, tiepre_location_t *);
void extract_temperatureST_tiempre(char *, tiepre_location_t *);
void extract_relative_humidity_tiempre(char *, tiepre_location_t *);
void extract_wind_tiempre(char *, tiepre_location_t *);
void extract_atmospheric_pressure_tiempre(char *, tiepre_location_t *);
char *extract_dynamic_string(char *);
char *copy_token_to_string(char *, size_t *);
void list_locations_telegram(tiepre_location_t *, uint64_t, char *, CURL *, string_buffer_t *);
int levenshtein_distance(const char *, const char *);
char *buscar_similitud(const char *, const tiepre_location_t *, uint16_t *);
uint32_t strcasecmp(const char *, const char *);

uint32_t malloc_count = 0;

int main() {
    char *dateApiUrl = "https://tools.aimylogic.com/api/now?tz=America/BuenosAires&format=dd/MM/yyyy";    // URL de donde extraer la fecha de hoy
    char *smnApiUrl = "https://ssl.smn.gob.ar/dpd/descarga_opendata.php?file=";                           // URL de donde extraer los datos
    char *telegramApiUrl = "https://api.telegram.org/bot<BOTID>/"; // URL a donde publicar los datos
    CURL *curlDateFile = NULL;                                                                            // Archivo(?¿?) de curl donde se guardan las configuraciones para la api de la fecha
    CURL *curlSmnFile = NULL;                                                                             // Archivo(?¿?) de curl donde se guardan las configuraciones para la api del smn
    CURL *curlTelegramFile = NULL;                                                                        // Archivo(?¿?) de curl donde se guardan las configuraciones para la api de telegram

    string_buffer_t dateApiResponse_s, smnApiResponse_s, telegramApiResponse_s; // Implementacion de String dinamica(?¿?) para guardar la respuesta de las solicitudes http
    init_buff(&dateApiResponse_s);                                              // Inicializacion de la String dinamica
    init_buff(&smnApiResponse_s);                                               // Inicializacion de la String dinamica
    init_buff(&telegramApiResponse_s);                                          // Inicializacion de la String dinamica

    curlDateFile = curl_init_date_api(dateApiUrl, &dateApiResponse_s);
    curlSmnFile = curl_init_smn_api(smnApiUrl, &smnApiResponse_s);
    curlTelegramFile = curl_init_telegram_api(telegramApiUrl, &telegramApiResponse_s);

    uint8_t kill = 0;
    while (kill == 0) {
        api_date_t date;
        request_date(curlDateFile, dateApiUrl, &dateApiResponse_s, &date);
        char updateID[20] = {0};
        char *newMessage = NULL;
        uint64_t chatID = 0;
        sprintf(updateID, "-1");
        wait_for_start_telegram(&chatID, updateID, newMessage, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
        free(newMessage);

        char *welecome = "_Bienvenido+al+bot+del+clima\\!_%0AEscribí+una+localidad+para+consultar+sus+condiciones+meteorológicas\\.%0APara+listar+todas+las+localidades+disponibles,+escribí+*\\.list*\\.";
        send_message_telegram(chatID, welecome, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);

        tiepre_location_t *tiepre = tiepre_request(smnApiUrl, curlSmnFile, &smnApiResponse_s, &date);

        uint8_t sts_exit = 0, gotLocMatch = 0;
        uint16_t pos = 0;
        char *res = NULL, *tmp = NULL;

        while (sts_exit == 0) {
            free(newMessage);
            newMessage = wait_new_message_telegram(chatID, updateID, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
            if (strcmp("diff-user", newMessage) != 0) {
                extract_telegram_message(&newMessage);
                printf("ID: %lu, Message: %s\n", chatID, newMessage);
                if (newMessage[0] == '.') {
                    if (strcmp(".list", newMessage) == 0) {
                        list_locations_telegram(tiepre, chatID, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
                    } else if (strcmp(".exit", newMessage) == 0) {
                        send_message_telegram(chatID, "Chau\\!", telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
                        sts_exit = 1;
                    } else if (strcmp(".kill", newMessage) == 0) {
                        send_message_telegram(chatID, "Chau\\!%0AKILLED", telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
                        sts_exit = 1;
                        kill = 1;
                    }
                    gotLocMatch = 0;
                } else if (strcasecmp(newMessage, "ok") == 0 && gotLocMatch) {
                    char *tiepreTable = calloc(1000, sizeof(char));
                    sprintf(tiepreTable, "Localidad: %s\nUltima actualizacion: %02hhu:%02hhu, %02hhu/%02hhu/%04hu\nPronostico actual: %s\nTemperatura: %.2fCº (ST %.2fCº)\nVisibilidad: %d Km\nHumedad: %d%%\nViento: %s Km/h\nPresion: %.2f KPa", tiepre[pos].location, tiepre[pos].hour, tiepre[pos].minute, tiepre[pos].day, tiepre[pos].month, tiepre[pos].year, tiepre[pos].status, tiepre[pos].temperature, tiepre[pos].temperatureST, tiepre[pos].visibility, tiepre[pos].relativeHumidity, tiepre[pos].windDirection, tiepre[pos].atmosphericPressure);
                    tmp = format_message_url(tiepreTable);
                    send_message_telegram(chatID, tmp, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
                    free(tmp);
                    free(tiepreTable);
                    gotLocMatch = 0;
                } else {
                    res = buscar_similitud(newMessage, tiepre, &pos);
                    tmp = format_message_url(res);
                    send_message_telegram(chatID, tmp, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
                    free(tmp);
                    tmp = format_message_url("Es esa la localidad que selecciono?\nSi es correcta, envie *OK* para confirmar, o escriba nuevamente para seleccionar otra localidad.");
                    send_message_telegram(chatID, tmp, telegramApiUrl, curlTelegramFile, &telegramApiResponse_s);
                    free(tmp);
                    gotLocMatch = 1;
                }
            }
        }
        free(newMessage);
        uint16_t locations = tiepre[0].tiepreLocations;
        for (int i = 0; i < locations; i++) {
            free(tiepre[i].location);
            free(tiepre[i].status);
            free(tiepre[i].windDirection);
        }
        free(tiepre);
        clear_buff(&smnApiResponse_s);
        clear_buff(&telegramApiResponse_s);
    }

    curl_easy_cleanup(curlDateFile);
    curl_easy_cleanup(curlSmnFile);
    curl_easy_cleanup(curlTelegramFile);
    free_buff(&dateApiResponse_s);
    free_buff(&smnApiResponse_s);
    free_buff(&telegramApiResponse_s);
    return 0;
}

void init_buff(string_buffer_t *buff) {
    buff->size = 0;                                      // primero, reseteamos la longitud del buffer
    buff->string = calloc(buff->size + 1, sizeof(char)); // le damos al puntero string len+1 bytes para guardar datos
    if (buff->string == NULL) {
        printf("MALLOC NULL\n");
        exit(1);
    }
    malloc_count += (buff->size + 1) * sizeof(char);
    buff->string[0] = '\0'; // nos aseguramos de que haya un \0 para saber cuando termina el string
}

void free_buff(string_buffer_t *buff) {
    free(buff->string);
    buff->size = 0;
    buff->string = NULL;
}

void clear_buff(string_buffer_t *buff) {
    free_buff(buff);
    init_buff(buff);
}

size_t write_callback(void *data, size_t size, size_t membc, void *sourceData) {
    size_t realsize = size * membc;                           // Calculamos el nuevo tamaño segun la cantidad de miembros por el tamaño?¿?¿?
    string_buffer_t *tmpbuff = (string_buffer_t *)sourceData; // Copiamos la direccion de memoria de los datos de origen a una para evitar
                                                              // hacer un cast a void cada vez que hacemos una asignacion a los datos de origen

    char *newData = realloc(tmpbuff->string, tmpbuff->size + realsize + 1);
    if (newData == NULL)
        exit(1);

    tmpbuff->string = newData;                                 // Copiamos la direccion de memoria donde se hizo el realloc con (tmpbuff->size + realsize + 1) bytes
    memcpy(&(tmpbuff->string[tmpbuff->size]), data, realsize); // Copiamos los datos nuevos a
    tmpbuff->size += realsize;                                 // Actualizamos el tamaño de los datos guardados en la estructura de origen
    tmpbuff->string[tmpbuff->size] = 0;                        // Aseguramos que hay un \0 al final del arreglo de datos
    return size * membc;
}

// Funcion para inicializar curl para la api de la fecha/hora
CURL *curl_init_date_api(char *dateApiUrl, string_buffer_t *dateApiResponse_s) {
    printf("init date\n");
    CURL *curlDateFile = curl_easy_init();
    if (!curlDateFile) {
        fprintf(stderr, "Fatal: curl_easy_init() error. curlDateFile\n");
        free_buff(dateApiResponse_s);
        exit(1);
    }
    curl_easy_setopt(curlDateFile, CURLOPT_URL, dateApiUrl);               // Definicion de URL para curl
    curl_easy_setopt(curlDateFile, CURLOPT_FOLLOWLOCATION, 1);             // Definicion para seguir redirecciones de la URL (1 = activadas)
    curl_easy_setopt(curlDateFile, CURLOPT_WRITEFUNCTION, write_callback); // Definicion para que libcurl sepa cual es la funcion a llamar cuando hay datos nuevos de entrada
    curl_easy_setopt(curlDateFile, CURLOPT_WRITEDATA, dateApiResponse_s);  // Definicion para que libcurl sepa la direccion de memoria de la string dinamica
    return curlDateFile;
}

// Funcion para inicializar curl para la api del SMN
CURL *curl_init_smn_api(char *smnApiUrl, string_buffer_t *smnApiResponse_s) {
    printf("init smn\n");
    CURL *curlSmnFile = curl_easy_init();
    if (!curlSmnFile) {
        fprintf(stderr, "Fatal: curl_easy_init() error. curlSmnFile\n");
        free_buff(smnApiResponse_s);
        exit(1);
    }
    // curl_easy_setopt(curlSmnFile, CURLOPT_URL, smnApiUrl);            // Definicion de URL para curl
    curl_easy_setopt(curlSmnFile, CURLOPT_FOLLOWLOCATION, 1);             // Definicion para seguir redirecciones de la URL (1 = activadas)
    curl_easy_setopt(curlSmnFile, CURLOPT_WRITEFUNCTION, write_callback); // Definicion para que libcurl sepa cual es la funcion a llamar cuando hay datos nuevos de entrada
    curl_easy_setopt(curlSmnFile, CURLOPT_WRITEDATA, smnApiResponse_s);   // Definicion para que libcurl sepa la direccion de memoria de la string dinamica
    return curlSmnFile;
}

// Funcion para inicializar curl para la api de telegram
CURL *curl_init_telegram_api(char *telegramApiUrl, string_buffer_t *telegramApiResponse_s) {
    printf("init telegram\n");
    CURL *curlTelegramFile = curl_easy_init();
    if (!curlTelegramFile) {
        fprintf(stderr, "Fatal: curl_easy_init() error. curlTelegramFile\n");
        free_buff(telegramApiResponse_s);
        exit(1);
    }
    // curl_easy_setopt(curlTelegramFile, CURLOPT_URL, telegramApiUrl);        // Definicion de URL para curl
    curl_easy_setopt(curlTelegramFile, CURLOPT_FOLLOWLOCATION, 1);                // Definicion para seguir redirecciones de la URL (1 = activadas)
    curl_easy_setopt(curlTelegramFile, CURLOPT_WRITEFUNCTION, write_callback);    // Definicion para que libcurl sepa cual es la funcion a llamar cuando hay datos nuevos de entrada
    curl_easy_setopt(curlTelegramFile, CURLOPT_WRITEDATA, telegramApiResponse_s); // Definicion para que libcurl sepa la direccion de memoria de la string dinamica
    return curlTelegramFile;
}

// Funcion para hacer solicitudes HTTP a la api de la fecha/hora
void request_date(CURL *curlDateFile, char *dateApiUrl, string_buffer_t *dateApiResponse_s, api_date_t *date) {
    printf("%s\n", dateApiUrl);
    CURLcode dateApiResponseCode = curl_easy_perform(curlDateFile);
    if (dateApiResponseCode != CURLE_OK) {
        fprintf(stderr, "Request failed: curl_easy_perform(): %s\n", curl_easy_strerror(dateApiResponseCode));
        curl_easy_cleanup(curlDateFile);
        free_buff(dateApiResponse_s);
        exit(1);
    }

    printf("%s\n\n%lu bytes recividos\n", dateApiResponse_s->string, (unsigned long)dateApiResponse_s->size);

    char *rcvd_date = strstr(dateApiResponse_s->string, "formatted");
    sscanf(rcvd_date, "formatted\":\"%hhu/%hhu/%hu\"", &date->day, &date->month, &date->year);
    printf("Fecha: Dia: %hhu, Mes: %hhu Anio:%hu\n\r", date->day, date->month, date->year);
}

// Funcion para hacer solicitudes HTTP a la api del SMN
void request_smn(char *smnDataRequest, char *smnApiUrl, CURL *curlSmnFile, string_buffer_t *smnApiResponse_s) {
    CURLcode smnApiResponseCode;
    char *fullSmnApiUrl = calloc(strlen(smnApiUrl) + strlen(smnDataRequest) + 1, sizeof(char));
    malloc_count += (strlen(smnApiUrl) + strlen(smnDataRequest) + 1) * sizeof(char);
    strcpy(fullSmnApiUrl, smnApiUrl);
    strcat(fullSmnApiUrl, smnDataRequest);
    printf("%s\n", fullSmnApiUrl);
    curl_easy_setopt(curlSmnFile, CURLOPT_URL, fullSmnApiUrl);
    smnApiResponseCode = curl_easy_perform(curlSmnFile);
    if (smnApiResponseCode != CURLE_OK) {
        fprintf(stderr, "Request failed: curl_easy_perform(): %s\n", curl_easy_strerror(smnApiResponseCode));
        free(fullSmnApiUrl);
        curl_easy_cleanup(curlSmnFile);
        free_buff(smnApiResponse_s);
        exit(1);
    }

    printf("%s\n\n%lu bytes recibidos\n", smnApiResponse_s->string, (unsigned long)smnApiResponse_s->size);
    free(fullSmnApiUrl);
}

// Funcion para hacer solicitudes HTTP a la api de telegram
void request_telegram(char *method, char *telegramApiUrl, CURL *curlTelegramFile, string_buffer_t *telegramApiResponse_s) {
    CURLcode telegramApiResponseCode;
    char *fullTelegramApiUrl = calloc(strlen(telegramApiUrl) + strlen(method) + 1, sizeof(char));
    malloc_count += strlen(telegramApiUrl) + strlen(method) + 1;
    strcpy(fullTelegramApiUrl, telegramApiUrl);
    strcat(fullTelegramApiUrl, method);
    printf("%s\n", fullTelegramApiUrl);
    curl_easy_setopt(curlTelegramFile, CURLOPT_URL, fullTelegramApiUrl);
    clear_buff(telegramApiResponse_s);
    telegramApiResponseCode = curl_easy_perform(curlTelegramFile);
    if (telegramApiResponseCode != CURLE_OK) {
        fprintf(stderr, "Request failed: curl_easy_perform(): %s\n", curl_easy_strerror(telegramApiResponseCode));
        curl_easy_cleanup(curlTelegramFile);
        free_buff(telegramApiResponse_s);
        exit(1);
    }

    printf("%s\n\n%ld bytes recibidos.\n", telegramApiResponse_s->string, telegramApiResponse_s->size);
    free(fullTelegramApiUrl);
}

// Funcion que espera que un nuevo mensaje llegue al chat de telegram
char *wait_new_message_telegram(uint64_t chatID, char *updateID, char *telegramApiUrl, CURL *curlTelegramFile, string_buffer_t *telegramApiResponse_s) {
    char *getUpdates = "getUpdates?offset=";
    char *method = update_update_id_method_telegram(updateID, getUpdates);
    request_telegram(method, telegramApiUrl, curlTelegramFile, telegramApiResponse_s);
    update_id_telegram(updateID, telegramApiResponse_s);

    char *inicio = NULL, *fin = NULL, *result = NULL;
    uint8_t userMessage = 0;
    do {
        free(method);
        method = update_update_id_method_telegram(updateID, getUpdates);
        request_telegram(method, telegramApiUrl, curlTelegramFile, telegramApiResponse_s);
        // printf("result:%s\nbytes: %01ld\n", result, strlen(result));
        inicio = strchr(telegramApiResponse_s->string, '[') + 1;
        fin = strchr(telegramApiResponse_s->string, ']');
        if (inicio != fin) {
            printf("New messages\n");
            result = calloc(fin - inicio + 1, sizeof(char));
            malloc_count = fin - inicio + 1;
            strncpy(result, inicio, fin - inicio);
            result[fin - inicio] = '\0';
            update_id_telegram(updateID, telegramApiResponse_s);
            userMessage = 1;
        }
        printf("\n\n");
    } while (userMessage == 0);
    if (chatID != 0) {
        if (chatID != extract_chat_id_telegram(result)) {
            printf("diff user\n");
            free(result);
            result = calloc(10, sizeof(char));
            malloc_count += 10;
            sprintf(result, "diff-user");
        }
    }
    free(method);
    return result;
}

// Funcion para actualizar el ID de ultima actualizacion de la cadena de metodo de telegram
char *update_update_id_method_telegram(char *updateID, char *method) {
    char *tmp_method = calloc(strlen(method) + strlen(updateID) + 1, sizeof(char));
    malloc_count += strlen(method) + strlen(updateID);
    strcpy(tmp_method, method);
    strcat(tmp_method, updateID);
    return tmp_method;
}

// Funcion para extraer el ID de ultima actualizacion de telegram
void update_id_telegram(char *updateID, string_buffer_t *telegramApiResponse_s) {
    uint32_t lastUpdateID = 0;
    char *inicio = NULL;
    inicio = strstr(telegramApiResponse_s->string, "{\"ok\":true,\"result\":[{\"update_id\":");
    if (inicio != NULL) {
        inicio += 34;
        lastUpdateID = atoi(inicio) + 1;
        printf("lastUpdateID: %u\n", lastUpdateID);
        sprintf(updateID, "%u", lastUpdateID);
    } else {
        printf("Couldn't find updateID\n");
    }
}

// Funcion para esperar que un cliente empiece una conversacion con el bot de telegram
void wait_for_start_telegram(uint64_t *chatID, char *updateID, char *newMessage, char *telegramApiUrl, CURL *curlTelegramFile, string_buffer_t *telegramApiResponse_s) {
    while (*chatID == 0) {
        newMessage = wait_new_message_telegram(0, updateID, telegramApiUrl, curlTelegramFile, telegramApiResponse_s);
        *chatID = extract_chat_id_telegram(newMessage);
    }
    extract_telegram_message(&newMessage);
    printf("ID: %lu, Message: %s\n", *chatID, newMessage);
    uint8_t started = 0;
    while (started == 0) {
        if (strcmp(".start", newMessage) == 0) {
            started = 1;
        } else {
            free(newMessage);
            newMessage = wait_new_message_telegram(*chatID, updateID, telegramApiUrl, curlTelegramFile, telegramApiResponse_s);
            if (strcmp("diff-user", newMessage) != 0) {
                extract_telegram_message(&newMessage);
                printf("ID: %lu, Message: %s\n", *chatID, newMessage);
            }
        }
    }
}

// Funcion para extraer el ID del chat de telegram
uint64_t extract_chat_id_telegram(char *telegramMessage) {
    uint64_t chatID = 0;
    char *inicio = NULL;
    inicio = strstr(telegramMessage, "\"chat\":{\"id\":");
    if (inicio != NULL) {
        inicio += 13;
        sscanf(inicio, "%lu", &chatID);
        printf("chatID: %ld\n", chatID);
        return chatID;
    } else {
        printf("Couldn't find chatID. Exiting...\n");
        exit(1);
    }
}

// Funcion para extraer mensajes del chat de telegram
void extract_telegram_message(char **telegramMessage) {
    char *inicio = NULL, *fin = NULL, *result = NULL;
    inicio = strstr(*telegramMessage, "\"text\":");
    if (inicio != NULL) {
        inicio += 8;
        fin = strrchr(inicio, '"');
        printf("%s, fin-inicio %ld\n", inicio, fin - inicio);
        if (inicio != fin) {
            result = calloc(fin - inicio + 1, sizeof(char));
            malloc_count = fin - inicio + 1;
            strncpy(result, inicio, fin - inicio + 1);
            result[fin - inicio] = '\0';
            printf("%s\n", result);
        }
        free(*telegramMessage);
        *telegramMessage = result;
    } else {
        printf("Received non-text message\n");
        free(*telegramMessage);
        *telegramMessage = calloc(10, sizeof(char));
        malloc_count += 10;
        sprintf(*telegramMessage, "non-text");
    }
}

// Funcion para enviar mensajes al chat de telegram
void send_message_telegram(uint64_t chatID, char *message, char *telegramApiUrl, CURL *curlTelegramFile, string_buffer_t *telegramApiResponse_s) {
    char *sendMessageA = "sendMessage?chat_id=", *sendMessageB = "&text=", *sendMessageC = "&parse_mode=MarkdownV2";
    char *method = calloc(strlen(sendMessageA) + count_digits(chatID) + strlen(sendMessageB) + strlen(message) + strlen(sendMessageC) + 1, sizeof(char));
    malloc_count += strlen(sendMessageA) + count_digits(chatID) + strlen(sendMessageB) + strlen(message) + strlen(sendMessageC) + 1;
    sprintf(method, "%s%lu%s%s%s", sendMessageA, chatID, sendMessageB, message, sendMessageC);
    request_telegram(method, telegramApiUrl, curlTelegramFile, telegramApiResponse_s);
    if (strncmp(telegramApiResponse_s->string, "{\"ok\":true,", 11) != 0) {
        printf("Something went wrong while sending the message! Exiting...");
        exit(1);
    }
    free(method);
}

// Funcion para cambiar caracteres especiales a caracteres compatibles con URL
char *format_message_url(char *message) {
    size_t size = strlen(message), newSize = 0;
    for (size_t i = 0; i < size; i++) {
        if (message[i] == '\n')
            newSize += 3;
        else if (strchr(".()-", message[i]) != NULL)
            newSize += 2;
        else
            newSize++;
    }
    char *formattedMessage = calloc(newSize + 1, sizeof(char));
    malloc_count = newSize + 1;

    uint32_t i = 0, j = 0;
    do {
        if (message[i] == ' ') {
            formattedMessage[j++] = '+';
        } else if (message[i] == '\n') {
            formattedMessage[j++] = '%';
            formattedMessage[j++] = '0';
            formattedMessage[j++] = 'A';
        } else if (strchr(".()-", message[i]) != NULL) {
            formattedMessage[j++] = '\\';
            formattedMessage[j++] = message[i];
        } else {
            formattedMessage[j++] = message[i];
        }
        i++;
    } while (i < size && j < newSize);
    formattedMessage[j] = '\0';
    printf("%s\nSize: %ld, length: %ld", formattedMessage, newSize, strlen(formattedMessage));
    return formattedMessage;
}

// Funcion para contar digitos de un numero
uint8_t count_digits(uint32_t n) {
    uint8_t count = 0;
    if (n == 0)
        return 1;

    while (n != 0) {
        n /= 10;
        count++;
    }
    return count;
}

// Funcion para convertir el texto del SMN de codificacion ISO8859 a UTF-8 (by Guillaume)
char *iconvISO2UTF8(char *iso) {
    iconv_t iconvDesc = iconv_open("UTF-8//TRANSLIT//IGNORE", "ISO−8859-1");

    if (iconvDesc == (iconv_t)-1) {
        /* Something went wrong.  */
        if (errno == EINVAL)
            fprintf(stderr, "conversion from '%s' to '%s' not available", "ISO−8859−1", "UTF-8");
        else
            fprintf(stderr, "LibIcon initialization failure");

        return NULL;
    }

    size_t iconv_value;
    char *utf8;
    size_t len;
    size_t utf8len;
    char *utf8start;

    len = strlen(iso);
    if (!len) {
        fprintf(stderr, "iconvISO2UTF8: input String is empty.");
        return NULL;
    }

    /* Assign enough space to put the UTF-8. */
    utf8len = 2 * len;
    utf8 = calloc(utf8len, sizeof(char));
    if (!utf8) {
        fprintf(stderr, "iconvISO2UTF8: Calloc failed.");
        return NULL;
    }
    /* Keep track of the variables. */
    utf8start = utf8;

    iconv_value = iconv(iconvDesc, &iso, &len, &utf8, &utf8len);
    /* Handle failures. */
    if (iconv_value == (size_t)-1) {
        switch (errno) {
                /* See "man 3 iconv" for an explanation. */
            case EILSEQ:
                fprintf(stderr, "iconv failed: Invalid multibyte sequence, in string '%s', length %d, out string '%s', length %d\n", iso, (int)len, utf8start, (int)utf8len);
                break;
            case EINVAL:
                fprintf(stderr, "iconv failed: Incomplete multibyte sequence, in string '%s', length %d, out string '%s', length %d\n", iso, (int)len, utf8start, (int)utf8len);
                break;
            case E2BIG:
                fprintf(stderr, "iconv failed: No more room, in string '%s', length %d, out string '%s', length %d\n", iso, (int)len, utf8start, (int)utf8len);
                break;
            default:
                fprintf(stderr, "iconv failed, in string '%s', length %d, out string '%s', length %d\n", iso, (int)len, utf8start, (int)utf8len);
        }
        return NULL;
    }

    if (iconv_close(iconvDesc) != 0) {
        fprintf(stderr, "libicon close failed: %s", strerror(errno));
    }

    return utf8start;
}

// Funcion para solicitar el tiepre al smn
tiepre_location_t *tiepre_request(char *smnApiUrl, CURL *curlSmnFile, string_buffer_t *smnApiResponse_s, api_date_t *date) {
    char smnDataRequest[50] = {0};
    sprintf(smnDataRequest, "observaciones/tiepre%04hu%02hhu%02hhu.txt", date->year, date->month, date->day);
    request_smn(smnDataRequest, smnApiUrl, curlSmnFile, smnApiResponse_s);
    char *utf8 = iconvISO2UTF8(smnApiResponse_s->string);
    clear_buff(smnApiResponse_s);
    smnApiResponse_s->string = utf8;
    smnApiResponse_s->size = strlen(utf8);
    return catalog_tiepre_request(smnApiResponse_s);
}

// Funcion para catalogar, ordenar y guardar la respuesta de smn
tiepre_location_t *catalog_tiepre_request(string_buffer_t *smnApiResponse_s) {
    uint16_t locCount = get_location_cuantity_tiempre(smnApiResponse_s);
    void (*extract[])(char *, tiepre_location_t *) = {extract_location_tiempre, extract_date_tiempre, extract_time_tiempre, extract_status_tiempre, extract_visibility_tiempre, extract_temperature_tiempre, extract_temperatureST_tiempre, extract_relative_humidity_tiempre, extract_wind_tiempre, extract_atmospheric_pressure_tiempre};

    tiepre_location_t *localTiepre = calloc(locCount + 1, sizeof(tiepre_location_t));
    malloc_count += (locCount + 1) * sizeof(tiepre_location_t);
    char *token = extract_dynamic_string(strtok(smnApiResponse_s->string, ";/"));
    for (int i = 0; i < locCount; i++) {
        for (int j = 0; j < 10; j++) {
            (*extract[j])(token, localTiepre + i);
            free(token);
            token = extract_dynamic_string(strtok(NULL, ";/"));
        }
        localTiepre[i].tiepreLocations = locCount - i;
        printf("vuelta %d\n\r", i);
        printf("%s, %hhu/%hhu/%hu, %hhu:%hhu, %s, %hhu Km, %5.2f Cº, %5.2f Cº, %hhu%% H/R, %hhu Km/h %s, %f KPa, vuelta %d\n", localTiepre[i].location, localTiepre[i].day, localTiepre[i].month, localTiepre[i].year, localTiepre[i].hour, localTiepre[i].minute, localTiepre[i].status, localTiepre[i].visibility, localTiepre[i].temperature, localTiepre[i].temperatureST, localTiepre[i].relativeHumidity, localTiepre[i].windSpeed, localTiepre[i].windDirection, localTiepre[i].atmosphericPressure, i);
    }
    for (int i = 0; i < locCount; i++) {
        printf("%s, %hhu/%hhu/%hu, %hhu:%hhu, %s, %hhu Km, %5.2f Cº, %5.2f Cº, %hhu%% H/R, %hhu Km/h %s, %f KPa\n", localTiepre[i].location, localTiepre[i].day, localTiepre[i].month, localTiepre[i].year, localTiepre[i].hour, localTiepre[i].minute, localTiepre[i].status, localTiepre[i].visibility, localTiepre[i].temperature, localTiepre[i].temperatureST, localTiepre[i].relativeHumidity, localTiepre[i].windSpeed, localTiepre[i].windDirection, localTiepre[i].atmosphericPressure);
    }
    return localTiepre;
}

// Funcion para saber la cantidad de localidades recibidas
uint16_t get_location_cuantity_tiempre(string_buffer_t *smnApiResponse_s) {
    uint16_t locCount = 0, byteCount = 0;
    while (smnApiResponse_s->string[byteCount] != '\0' && byteCount < smnApiResponse_s->size) {
        if (smnApiResponse_s->string[byteCount] == '/')
            locCount++;
        byteCount++;
    }
    printf("%d localidades encontradas!\n\r", locCount);
    return locCount;
}

// Funcion para extraer el nombre de la localidad
void extract_location_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    if (token[0] == ' ')
        token += 3;
    remoteTiepre->location = copy_token_to_string(token, &remoteTiepre->locStrSize);
    printf("primer token: %s\n\r", remoteTiepre->location);
}

// Funcion para extraer la hora de actualizacion
void extract_date_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    char month[11] = {0};
    sscanf(token, "%hhu-%[^-]-%hu", &remoteTiepre->day, month, &remoteTiepre->year);
    printf("%hhu-%s-%hu\n", remoteTiepre->day, month, remoteTiepre->year);
    for (int j = 0; j < 12; j++) {
        if (!strcmp(month, meses[j])) {
            remoteTiepre->month = j + 1;
            break;
        }
    }
    printf("segundo token: %hhu/%hhu/%hu\n\r", remoteTiepre->day, remoteTiepre->month, remoteTiepre->year);
}

// Funcion para extraer la fecha de actualizacion
void extract_time_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    sscanf(token, "%hhu:%hhu", &remoteTiepre->hour, &remoteTiepre->minute);
    printf("tercer token: %hhu:%hhu\n\r", remoteTiepre->hour, remoteTiepre->minute);
}

// Funcion para extraer el estado
void extract_status_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->status = copy_token_to_string(token, &remoteTiepre->stsStrSize);
    printf("cuarto token: %s\n\r", remoteTiepre->status);
}

// Funcion para extraer la distancia de visibilidad
void extract_visibility_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->visibility = (uint8_t)atoi(token);
    printf("quinto token\n\r");
}

// Funcion para extraer la temperatura
void extract_temperature_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->temperature = atof(token);
    printf("sexto token\n\r");
}

// Funcion para extraer la sensacion termica
void extract_temperatureST_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->temperatureST = atof(token);
    printf("septimo token\n\r");
}

// Funcion para extraer la humadad relativa
void extract_relative_humidity_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->relativeHumidity = (uint8_t)atoi(token);
    printf("octavo token\n\r");
}

// Funcion para extraer datos del viento
void extract_wind_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->windDirection = copy_token_to_string(token, &remoteTiepre->windDirStrSize);
    printf("noveno token\n\r");
}

// Funcion para extraer la presion atmosferica
void extract_atmospheric_pressure_tiempre(char *token, tiepre_location_t *remoteTiepre) {
    remoteTiepre->atmosphericPressure = atof(token);
    printf("decimo token\n\r");
}

// Funcion para extraer las cadenas de la respuesta del SMN
char *extract_dynamic_string(char *token) {
    char *tmp = calloc(strlen(token) + 1, sizeof(char));
    malloc_count += strlen(token) + 1;
    strcpy(tmp, token);
    return tmp;
}

// Funcion para copiar el token de strtok a otra direccion de memoria fuera del alcance de strtok
char *copy_token_to_string(char *token, size_t *newSize) {
    char *string;
    *newSize = strlen(token) + 1;
    printf("pre. len: %ld, token: %s. malloc_count: %d\n", *newSize, token, malloc_count);
    if (newSize == NULL) {
        printf("newSize NULL!!! KILLING\n");
        exit(1);
    }
    string = calloc(*newSize + 1, sizeof(char));
    malloc_count += *newSize + 1;
    strcpy(string, token);
    string[*newSize] = 0;
    printf("post. string: %s\n", string);
    return string;
}

// Funcion para listar las localidades disponibles al chat de telegram
void list_locations_telegram(tiepre_location_t *remoteTiepre, uint64_t chatID, char *telegramApiUrl, CURL *curlTelegramFile, string_buffer_t *telegramApiResponse_s) {
    uint16_t charCount = 0;
    for (int i = 0; i < remoteTiepre[0].tiepreLocations; i++) {
        charCount += remoteTiepre[i].locStrSize;
    }
    char *locations = calloc(charCount + 1, sizeof(char));
    malloc_count += charCount + 1;
    uint16_t charCountTotal = 0;
    for (int i = 0; i < remoteTiepre[0].tiepreLocations; i++) {
        uint8_t charCountInvividual = 0;
        do {
            locations[charCountTotal] = remoteTiepre[i].location[charCountInvividual];
            charCountInvividual++;
            charCountTotal++;
        } while (remoteTiepre[i].location[charCountInvividual] != 0);
        locations[charCountTotal] = '\n';
        charCountTotal++;
    }
    locations[charCountTotal] = '\0';
    printf("%s\nAllocated: %d, Used: %d\n", locations, charCount, charCountTotal);
    char *formattedLocations = format_message_url(locations);
    send_message_telegram(chatID, formattedLocations, telegramApiUrl, curlTelegramFile, telegramApiResponse_s);
    free(locations);
    free(formattedLocations);
}

// Función para calcular la distancia de Levenshtein entre dos cadenas (by chatgpt)
int levenshtein_distance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    int matrix[len1 + 1][len2 + 1];

    for (int i = 0; i <= len1; ++i) {
        for (int j = 0; j <= len2; ++j) {
            if (i == 0) {
                matrix[i][j] = j;
            } else if (j == 0) {
                matrix[i][j] = i;
            } else {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                matrix[i][j] = fmin(fmin(matrix[i - 1][j] + 1, matrix[i][j - 1] + 1), matrix[i - 1][j - 1] + cost);
            }
        }
    }

    return matrix[len1][len2];
}

// Función para encontrar la cadena más similar en un arreglo de cadenas (by chatgpt)
char *buscar_similitud(const char *cadena, const tiepre_location_t *remoteTiepre, uint16_t *pos) {
    int mejor_similitud = -1;
    char *resultado = NULL;

    for (int i = 0; i < remoteTiepre[0].tiepreLocations; ++i) {
        int similitud = levenshtein_distance(cadena, remoteTiepre[i].location);

        if (mejor_similitud == -1 || similitud < mejor_similitud) {
            mejor_similitud = similitud;
            resultado = remoteTiepre[i].location;
            *pos = i;
        }
    }

    return resultado;
}

// Funcion para comparar cadenas sin diferenciar mayusculas de minusculas (xnu-kernel)
uint32_t strcasecmp(const char *s1, const char *s2) {
    const unsigned char *us1 = (const unsigned char *)s1,
                        *us2 = (const unsigned char *)s2;

    while (tolower(*us1) == tolower(*us2++))
        if (*us1++ == '\0')
            return (0);
    return (tolower(*us1) - tolower(*--us2));
}
