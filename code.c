#include <stdio.h>        /* printf, perror */
#include <stdlib.h>       /* exit */
#include <fcntl.h>        /* open */
#include <unistd.h>       /* read, write, close */
#include <sys/ioctl.h>    /* ioctl */
#include <linux/i2c-dev.h>/* I2C_SLAVE */
#include <wiringPi.h>     /* GPIO kontrola - digitalWrite, pinMode... */
#include <lcd.h>          /* LCD kontrola - lcdInit, lcdPuts... */
#include <signal.h>       /* signal - hvatanje Ctrl+C */

/* I2C adresa PCF8591 ADC konvertora - odredjena hardverski (A0,A1,A2 = GND) */
#define ADRESA 0x48

/*
 * WiringPi numeracija pinova - RAZLICITA od GPIO numeracije!
 * Koristiti "gpio readall" komandu za pregled svih pinova i njihovih brojeva
 */
#define BAZ_pin    27  /* GPIO 16, fizicki pin 36 - buzzer (active low: LOW=ukljucen) */
#define LED_ZELENA  0  /* GPIO 17, fizicki pin 11 - zelena LED (cistvazduh) */
#define LED_ZUTA    2  /* GPIO 27, fizicki pin 13 - zuta LED (upozorenje) */
#define LED_CRVENA  3  /* GPIO 22, fizicki pin 15 - crvena LED (opasnost) */
#define MOTOR       1  /* GPIO 18, fizicki pin 12 - DC motor/ventilator */

/* Globalne promenljive - moraju biti globalne da bi cleanup() mogla da ih koristi */
int fd;   /* fajl deskriptor za I2C komunikaciju sa /dev/i2c-1 */
int LCD;  /* handle koji vraca lcdInit(), koristi se za sve LCD operacije */

/*
 * cleanup() - funkcija koja se automatski poziva kada korisnik pritisne Ctrl+C
 * ili kada program dobije SIGTERM signal (npr. kill komanda)
 * Njena uloga je da uredno ugasi sve komponente pre izlaska iz programa,
 * kako ne bi ostale upaljene nakon sto program prestane da radi
 */
void cleanup(int sig)
{
    lcdClear(LCD);                  /* ocisti LCD ekran */
    digitalWrite(LED_ZELENA, LOW);  /* ugasi zelenu LED */
    digitalWrite(LED_ZUTA, LOW);    /* ugasi zutu LED */
    digitalWrite(LED_CRVENA, LOW);  /* ugasi crvenu LED */
    digitalWrite(BAZ_pin, HIGH);    /* ugasi buzzer - HIGH jer je active low */
    digitalWrite(MOTOR, LOW);       /* ugasi DC motor */
    close(fd);                      /* zatvori I2C konekciju */
    exit(0);                        /* izadji iz programa */
}

/*
 * setLed() - pomocna funkcija za istovremenu kontrolu sve tri LED diode
 * Uvek prima vrednosti za sve tri LED odjednom (HIGH ili LOW)
 * Na ovaj nacin se sprecava da vise LED svetle istovremeno
 */
void setLed(int zelena, int zuta, int crvena)
{
    digitalWrite(LED_ZELENA, zelena);
    digitalWrite(LED_ZUTA, zuta);
    digitalWrite(LED_CRVENA, crvena);
}

int main()
{
		int thresh1 = 45;		// prag 1
    int thresh2 = 55;		// prag 2
    /* Registruj cleanup() funkciju koja ce se pozvati pri prekidu programa */
    signal(SIGINT, cleanup);   /* SIGINT  = Ctrl+C */
    signal(SIGTERM, cleanup);  /* SIGTERM = kill komanda */

    /* Inicijalizacija wiringPi biblioteke za kontrolu GPIO pinova */
    if (wiringPiSetup() == -1) exit(1);

    /* Postavi sve pinove kao izlazne (OUTPUT) */
    pinMode(BAZ_pin, OUTPUT);
    pinMode(LED_ZELENA, OUTPUT);
    pinMode(LED_ZUTA, OUTPUT);
    pinMode(LED_CRVENA, OUTPUT);
    pinMode(MOTOR, OUTPUT);

    /* Pocetno stanje - sve komponente ugasene pri pokretanju programa */
    digitalWrite(BAZ_pin, HIGH);  /* HIGH = ugasen (buzzer je active low) */
    digitalWrite(MOTOR, LOW);     /* motor ugasen */
    setLed(LOW, LOW, LOW);        /* sve tri LED ugasene */

    /*
     * Inicijalizacija LCD displeja
     * lcdInit(redova, kolona, bitovi, RS, E, D4, D5, D6, D7, ...)
     * Koristimo 4-bitni mod sa pinovima: RS=25, E=24, D4=23, D5=22, D6=21, D7=14
     */
    LCD = lcdInit(2, 16, 4, 25, 24, 23, 22, 21, 14, 0, 0, 0, 0);
    if (LCD == -1) { printf("LCD inicijalizacija neuspesna!\n"); exit(2); }

    /* Otvaranje I2C busa kao fajla - Linux tretira uredjaje kao fajlove */
    fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) { perror("i2c open"); exit(1); }

    /* Postavljanje I2C adrese uredjaja sa kojim komuniciramo (PCF8591 = 0x48) */
    if (ioctl(fd, I2C_SLAVE, ADRESA) < 0) { perror("ioctl"); exit(1); }

    /* Konfiguracija PCF8591 - vrednost 0x00 bira kanal AIN0 za ocitavanje */
    unsigned char config = 0x00;
    write(fd, &config, 1);

    /* Ispisi naslov na prvom redu LCD-a - radi se samo jednom, van petlje */
    lcdPosition(LCD, 0, 0);
    lcdPuts(LCD, "Rezultat je:    ");

    /* Glavna petlja - stalno ocitava senzor i reaguje na promene */
    while (1)
    {
        unsigned char val;  /* promenljiva za cuvanje vrednosti sa senzora (0-255) */

        /*
         * PCF8591 radi sa odmakom - uvek vraca vrednost PRETHODNE konverzije.
         * Resenje: citamo dva puta uz pauzu izmedju:
         *   1. prvo citanje - pokrace novu konverziju, rezultat ignorisemo
         *   2. delay(10)    - cekamo 10ms da ADC zavrsi konverziju
         *   3. drugo citanje - sada dobijamo stvarnu trenutnu vrednost
         */
        read(fd, &val, 1);  /* prvo citanje - baci, ovo je stara vrednost */
        delay(10);          /* sacekaj da ADC zavrsi konverziju */
        read(fd, &val, 1);  /* drugo citanje - ovo je stvarna trenutna vrednost */

        /* Ispisi vrednost u terminal za pracenje i debug */
        printf("Vrednost: %d\n", val);

        /*
         * Odluka na osnovu vrednosti senzora - tri nivoa opasnosti:
         *
         * NIVO 1 - Cistvazduh (val <= 45):
         *   Zelena LED svetli, buzzer i motor su iskljuceni
         *
         * NIVO 2 - Upozorenje (46 <= val <= 55):
         *   Zuta LED svetli kao upozorenje, buzzer i motor su jos uvek iskljuceni
         *
         * NIVO 3 - Opasnost (val > 55):
         *   Crvena LED svetli, buzzer pisti, motor se pali i provetrava prostoriju
         *   Sistem automatski prelazi u nizi nivo cim vrednost padne ispod praga
         */
        if (val <= thresh1)
        {
            setLed(HIGH, LOW, LOW);       /* upali zelenu, ugasi ostale */
            digitalWrite(BAZ_pin, HIGH);  /* ugasi buzzer */
            digitalWrite(MOTOR, LOW);     /* ugasi motor */
        }
        else if (val <= thresh2)
        {
            setLed(LOW, HIGH, LOW);       /* upali zutu, ugasi ostale */
            digitalWrite(BAZ_pin, HIGH);  /* buzzer jos uvek ugasen */
            digitalWrite(MOTOR, LOW);     /* motor jos uvek ugasen */
        }
        else
        {
            setLed(LOW, LOW, HIGH);       /* upali crvenu, ugasi ostale */
            digitalWrite(BAZ_pin, LOW);   /* LOW = ukljucen (active low buzzer) */
            digitalWrite(MOTOR, HIGH);    /* HIGH = upali ventilator */
        }

        /*
         * Ispisi vrednost na drugi red LCD-a
         * "%-16d" - levo poravnat broj sa padding-om do 16 znakova
         * Na ovaj nacin precrtavamo staru vrednost bez lcdClear() koji treperi
         */
        lcdPosition(LCD, 0, 1);
        lcdPrintf(LCD, "%-16d", val);

        delay(500);  /* pauza 500ms pre sledeceg ocitavanja */
    }

    close(fd);  /* zatvori I2C (nikad se ne dostize zbog while(1), ali dobra praksa) */
    return 0;
}
