#ifdef _WIN32
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <stdbool.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/select.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <errno.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <stdbool.h>
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
// Stuur een string (zonder null-terminator) over de UDP verbinding.
static void send_msg( socket_t sock, const char * msg,
                      const struct sockaddr * addr, socklen_t len )
{
    if( sendto( sock, msg, (int)strlen( msg ), 0, addr, len ) == SOCKET_ERROR_VALUE )
        print_error( "sendto" );
    else
        printf( "  >> Verstuurd naar client: \"%s\"\n", msg );
}
// Probeer een pakket te ontvangen binnen de resterende tijd tot 'deadline'.
static socket_t make_socket( void )
{
    struct addrinfo hints, *res;
    memset( &hints, 0, sizeof hints );
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE;

    if( getaddrinfo( NULL, "5500", &hints, &res ) != 0 )
    { fprintf( stderr, "getaddrinfo mislukt\n" ); exit( 1 ); }

    socket_t s = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
    if( s == INVALID_SOCKET_VALUE ) { print_error( "socket" ); exit( 1 ); }
    // Optie om adres direct te hergebruiken (handig bij snel opnieuw starten van server)
    int reuse = 1;
    setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof reuse );
    // Bind de socket aan het opgegeven adres en poort. Dit maakt de socket klaar om inkomende pakketten te ontvangen op dat adres en poort.
    if( bind( s, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR_VALUE )
    { print_error( "bind" ); exit( 1 ); }

    freeaddrinfo( res );
    printf( "Server luistert op poort 5500\n" );
    return s;
}

/*
  Probeer een pakket te ontvangen binnen de resterende tijd tot 'deadline'.
  Geeft aantal bytes terug, 0 = timeout verstreken, -1 = fout.
 */
static int recv_until( socket_t sock, char * buf, int bufsz,
                        struct sockaddr_storage * src, socklen_t * srclen,
                        time_t deadline )
{
    time_t now = time( NULL );
    if( now >= deadline ) return 0;
    // Gebruik select met korte timeout om te wachten op inkomende pakketten, maar niet langer dan de resterende tijd tot 'deadline'.
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( sock, &fds );
    struct timeval tv;
    tv.tv_sec  = (long)( deadline - now );
    tv.tv_usec = 0;

    int r = select( (int)sock + 1, &fds, NULL, NULL, &tv );
    if( r <= 0 ) return 0;

    *srclen = sizeof( struct sockaddr_storage );
    int n = recvfrom( sock, buf, bufsz - 1, 0, (struct sockaddr *)src, srclen );
    if( n == SOCKET_ERROR_VALUE ) { print_error( "recvfrom" ); return -1; }
    buf[n] = '\0';
    return n;
}

void execution( socket_t sock )
{
    srand( (unsigned int)time( NULL ) );
    char buf[256];

    while( true )   // herhaal voor elke ronde 
    {
        printf( "\n========================================\n" );
        printf( "Wachten op eerste gok van de client...\n" );

        
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof client_addr;

        /* Blokkeer totdat de eerste gok binnenkomt (geen timeout hier) */
        int n = recvfrom( sock, buf, sizeof buf - 1, 0,
                          (struct sockaddr *)&client_addr, &client_len );
        if( n <= 0 ) continue;
        buf[n] = '\0';

        /* Genereer random doelgetal */
        int target = rand() % 101;
        printf( "Eerste gok ontvangen: \"%s\"\n", buf );
        printf( "Random getal gegenereerd: %d\n", target );

        /* Sla beste gok op */
        int best_guess    = atoi( buf );
        int best_distance = abs( best_guess - target );
        struct sockaddr_storage best_addr = client_addr;
        socklen_t best_len = client_len;
        printf( "Beste gok: %d (afstand %d)\n", best_guess, best_distance );

        /* Start timer van 8 seconden */
        double timeout_sec = 8.0;
        time_t timer_start = time( NULL );
        time_t deadline    = timer_start + (time_t)timeout_sec;
        printf( "Timer gestart: %.0f seconden\n", timeout_sec );

       
        while( true )
        {
            struct sockaddr_storage src;
            socklen_t srclen = sizeof src;

            int bytes = recv_until( sock, buf, sizeof buf, &src, &srclen, deadline );

            if( bytes == 0 )
            {
                /* Timer verlopen */
                printf( "Timer verlopen.\n" );
                break;
            }
            if( bytes < 0 ) continue;

            /* Nieuwe gok ontvangen */
            int guess    = atoi( buf );
            int distance = abs( guess - target );
            printf( "Nieuwe gok ontvangen: %d (afstand %d)\n", guess, distance );

            if( distance < best_distance )
            {
                best_guess    = guess;
                best_distance = distance;
                best_addr     = src;
                best_len      = srclen;
                printf( "Nieuwe beste gok: %d (afstand %d)\n", best_guess, best_distance );
            }

            /* Halveer de resterende tijd en herstart de timer */
            time_t now       = time( NULL );
            double remaining = (double)( deadline - now );
            if( remaining < 0 ) remaining = 0;
            timeout_sec = remaining / 2.0;
            timer_start = time( NULL );
            deadline    = timer_start + (time_t)( timeout_sec + 0.5 );
            printf( "Timer gehalveerd en herstart: %.1f seconden resterend\n", timeout_sec );
        }

        // Stuur resultaat naar beste gok en start 16s timer voor late berichten
        printf( "Stuur \"You won ?\" naar de winnaar (gok %d, afstand %d)\n",
                best_guess, best_distance );
        send_msg( sock, "You won ?", (struct sockaddr *)&best_addr, best_len );
        // Start timer van 16 seconden voor late berichten
        time_t deadline2    = time( NULL ) + 16;
        bool got_late_msg   = false;
        printf( "Timer gestart: 16 seconden\n" );
        // Blijf luisteren naar late berichten tot 16s voorbij, stuur "You lost !" voor elk bericht dat binnenkomt
        while( true )
        {
            struct sockaddr_storage src;
            socklen_t srclen = sizeof src;

            int bytes = recv_until( sock, buf, sizeof buf, &src, &srclen, deadline2 );

            if( bytes == 0 )
            {
                /* 16 seconden voorbij, niemand heeft meer iets gestuurd */
                printf( "16 seconden verstreken, geen bericht meer ontvangen.\n" );
                break;
            }
            if( bytes < 0 ) continue;

            /* Er is een bericht ontvangen binnen de 16 seconden */
            got_late_msg = true;
            printf( "Bericht ontvangen binnen 16s: \"%s\" -> stuur \"You lost !\"\n", buf );
            send_msg( sock, "You lost !", (struct sockaddr *)&src, srclen );
            /* Blijf de rest van de 16 seconden luisteren (alle berichten krijgen "You lost !") */
        }

        if( !got_late_msg )
        {
            /* Geen bericht meer binnen 16s -> bevestig winst */
            printf( "Geen bericht meer binnen 16s -> stuur \"You won !\"\n" );
            send_msg( sock, "You won !", (struct sockaddr *)&best_addr, best_len );
        }

        printf( "\nRonde afgelopen. Klaar voor volgende ronde.\n" );
    }
}

void cleanup( socket_t sock )
{
    CLOSE_SOCKET( sock );
}

int main( void )
{
    OSInit();
    socket_t sock = make_socket();
    execution( sock );
    cleanup( sock );
    OSCleanup();
    return 0;
}
