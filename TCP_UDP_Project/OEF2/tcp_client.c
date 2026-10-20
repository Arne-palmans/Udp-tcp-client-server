#ifdef _WIN32
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <stdint.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <errno.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

static void OSInit( void )
{
#ifdef _WIN32
    WSADATA wsaData;
    int e = WSAStartup( MAKEWORD( 2, 0 ), &wsaData );
    if( e != 0 ) { fprintf( stderr, "WSAStartup: %d\n", e ); exit( -1 ); }
#endif
}

static void OSCleanup( void )
{
    //activeerd cleanup van de Windows Sockets API, waarbij alle resources worden vrijgegeven die door de API zijn toegewezen.
#ifdef _WIN32
    WSACleanup();
#endif
}

static void print_error( const char * ctx )
{
#ifdef _WIN32
    fprintf( stderr, "%s: WSA errno = %d\n", ctx, WSAGetLastError() );
#else
    perror( ctx );
#endif
}

/* Maak TCP verbinding met de server op 127.0.0.1:5500 */
static socket_t connect_to_server( void )
{
    struct addrinfo hints, *res;
    memset( &hints, 0, sizeof hints );
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if( getaddrinfo( "127.0.0.1", "5500", &hints, &res ) != 0 )
    { fprintf( stderr, "getaddrinfo mislukt\n" ); exit( 1 ); }

    socket_t s = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
    if( s == INVALID_SOCKET_VALUE ) { print_error( "socket" ); exit( 1 ); }

    if( connect( s, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR_VALUE )
    { print_error( "connect" ); exit( 1 ); }

    freeaddrinfo( res );
    printf( "Verbonden met server!\n" );
    return s;
}

/*
  Stuur een 32-bit integer in network-byte-order naar de server.
 */
static int send_int32( socket_t sock, int value )
{
    // Converteer de integer naar network-byte-order (big-endian) voordat je deze verstuurt.
    uint32_t net_val = htonl( (uint32_t)value );
    // Stuur de 4 bytes van de geconverteerde integer naar de server.
    int sent = send( sock, (const char *)&net_val, 4, 0 );
    // Controleer of het verzenden succesvol was. Als send een fout retourneert, geef dan een foutmelding weer.
    if( sent == SOCKET_ERROR_VALUE )
    // print_error geeft een foutmelding weer met de context "send" en de specifieke foutcode van de Windows Sockets API.
    { print_error( "send" ); return 0; }
    return 1;
}

//functie die probeert een regel tekst van de gebruiker te lezen zonder te blokkeren.
static int recv_string( socket_t sock, char * buf, int bufsz )
{
    int total = 0;
    while( total < bufsz - 1 )
    {
        /* Gebruik select met korte timeout om te detecteren dat het bericht compleet is */
        fd_set fds;
        FD_ZERO( &fds );
        FD_SET( sock, &fds );
        struct timeval tv = { 0, 200000 };   /* 200 ms wachten op meer data */
        int r = select( (int)sock + 1, &fds, NULL, NULL, &tv );

        if( r == 0 ) break;   /* geen data meer, bericht is compleet */
        if( r < 0 ) { print_error( "select" ); break; }

        int n = recv( sock, buf + total, bufsz - 1 - total, 0 );
        if( n <= 0 )
        {
            if( n == 0 && total == 0 )
                return 0;   /* verbinding verbroken zonder data */
            break;
        }
        total += n;
    }

    buf[total] = '\0';
    // Print de ontvangen string voor debugdoeleinden.
    return total;
}

int main( void )
{
    OSInit();

    while( 1 )   /* herhaal voor elke ronde */
    {
        printf( "\n========================================\n" );
        printf( "Druk Enter om te verbinden, of typ 'stop' om te stoppen: " );
        fflush( stdout );

        char line[64];
        if( fgets( line, sizeof line, stdin ) == NULL ) break;
        line[ strcspn( line, "\r\n" ) ] = '\0';
        if( strcmp( line, "stop" ) == 0 ) break;

        /* Maak TCP verbinding (= "Connect" in het schema) */
        socket_t sock = connect_to_server();

        printf( "Raad het getal (1-1000000).\n" );

        /* Spelloop: stuur gokken, ontvang hints, stop bij "Correct" of disconnect */
        while( 1 )
        {
            /* Vraag de gebruiker om een getal */
            printf( "Jouw gok: " );
            fflush( stdout );

            if( fgets( line, sizeof line, stdin ) == NULL ) goto disconnect;
            line[ strcspn( line, "\r\n" ) ] = '\0';

            /* Valideer invoer */
            if( line[0] == '\0' ) continue;
            char * endptr;
            long val = strtol( line, &endptr, 10 );
            if( *endptr != '\0' )
            {
                printf( "Ongeldige invoer — geef een integer.\n" );
                continue;
            }

            /* Stuur integer in network-byte-order naar server */
            if( !send_int32( sock, (int32_t)val ) ) goto disconnect;
            printf( "Verstuurd: %ld (als 32-bit network-byte-order)\n", val );

            /* Ontvang antwoord van server */
            char response[64];
            int n = recv_string( sock, response, sizeof response );

            if( n == 0 )
            {
                /* Server heeft verbinding verbroken zonder antwoord */
                printf( "Server heeft verbinding verbroken.\n" );
                break;
            }

            printf( "Server: %s\n", response );

            /* Bij "Correct": server verbreekt daarna de verbinding */
            if( strcmp( response, "Correct" ) == 0 )
            {
                printf( "Gefeliciteerd! Je hebt het getal geraden.\n" );
                break;
            }
        }

disconnect:
        /* Verbreek verbinding (= "Disconnect" in het schema) */
        CLOSE_SOCKET( sock );
        printf( "Verbinding verbroken.\n" );
    }

    OSCleanup();
    return 0;
}
