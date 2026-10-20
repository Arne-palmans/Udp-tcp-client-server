#ifdef _WIN32
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <stdint.h>
    #include <process.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    typedef unsigned long thread_t;
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
    #include <time.h>
    #include <unistd.h>
    #include <pthread.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    typedef pthread_t thread_t;
#endif

/* Structuur voor thread-gegevens */
typedef struct {
    socket_t client_sock;
    int client_id;
} client_thread_args_t;

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

/* Maak luisterende TCP socket op poort 5500 */
static socket_t make_listen_socket( void )
{
    struct addrinfo hints, *res;
    memset( &hints, 0, sizeof hints );
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if( getaddrinfo( NULL, "5500", &hints, &res ) != 0 )
    { fprintf( stderr, "getaddrinfo mislukt\n" ); exit( 1 ); }
    // Maak TCP socket en controleer op fouten
    socket_t s = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
    if( s == INVALID_SOCKET_VALUE ) { print_error( "socket" ); exit( 1 ); }
    // Optie om adres direct te hergebruiken
    int reuse = 1;
    setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof reuse );

    if( bind( s, res->ai_addr, res->ai_addrlen ) == SOCKET_ERROR_VALUE )
    { print_error( "bind" ); exit( 1 ); }

    if( listen( s, 1 ) == SOCKET_ERROR_VALUE )
    { print_error( "listen" ); exit( 1 ); }

    freeaddrinfo( res );
    printf( "TCP-server luistert op poort 5500...\n" );
    return s;
}

/*
  Stuur een string over de TCP verbinding.
  Geeft false terug bij fout.
 */
static int send_string( socket_t client, const char * msg )
{
    int len = (int)strlen( msg );
    int sent = send( client, msg, len, 0 );
    if( sent == SOCKET_ERROR_VALUE )
    { print_error( "send" ); return 0; }
    printf( "  >> Verstuurd: \"%s\"\n", msg );
    return 1;
}

/*
  Ontvang precies 4 bytes (een 32-bit integer in network-byte-order).
  Geeft false terug bij verbindingsverbreking of fout.
 */
static int recv_int32( socket_t client, int32_t * out )
{
    uint32_t net_val;
    int total = 0;
    while( total < 4 )
    {
        int n = recv( client, (char *)&net_val + total, 4 - total, 0 );
        if( n <= 0 )
        {
            if( n == 0 )
                printf( "Client heeft de verbinding verbroken.\n" );
            else
                print_error( "recv" );
            return 0;
        }
        total += n;
    }
    *out = (int32_t)ntohl( net_val );
    return 1;
}

/*
 
   1. Genereer random getal.
   2. Ontvang gok van client (integer in network-byte-order).
   3. Stuur "Hoger", "Lager" of "Correct" terug als string.
   4. Bij "Correct": disconnect.
   5. Bij disconnect van client: sessie klaar.
   
   Deze functie wordt in een aparte thread uitgevoerd voor elke client.
 */
#ifdef _WIN32
static unsigned int __stdcall handle_client_thread( void * arg )
#else
static void * handle_client_thread( void * arg )
#endif
{
    client_thread_args_t * args = (client_thread_args_t *)arg;
    socket_t client = args->client_sock;
    int client_id = args->client_id;
    free( args );

    srand( (unsigned int)time( NULL ) + client_id );
    int target = ( rand() % 1000000 ) + 1;
    printf( "[Client %d] Random getal gegenereerd: %d\n", client_id, target );
// Spelloop: ontvang gokken en stuur hints totdat correct of disconnect
    while( 1 )
    {
        /* Ontvang integer in network-byte-order */
        int32_t guess;
        if( !recv_int32( client, &guess ) )
        {
            /* Client heeft verbinding verbroken (zoals in schema scenario 2) */
            printf( "[Client %d] Verbinding verbroken door client.\n", client_id );
            break;
        }
        printf( "[Client %d] Gok ontvangen: %d\n", client_id, guess );

        /* Vergelijk en stuur antwoord */
        if( guess < target )
        {
            if( !send_string( client, "Hoger" ) ) break;
        }
        else if( guess > target )
        {
            if( !send_string( client, "Lager" ) ) break;
        }
        else
        {
            /* Correct! Stuur "Correct" en verbreek dan zelf de verbinding */
            send_string( client, "Correct" );
            printf( "[Client %d] Correct geraden! Server verbreekt verbinding.\n", client_id );
            break;
        }
    }

    CLOSE_SOCKET( client );
    printf( "[Client %d] Verbinding gesloten.\n", client_id );
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int main( void )
{
    OSInit();
    socket_t listen_sock = make_listen_socket();
    int client_counter = 0;

    // Hoofdloop: accepteer verbindingen en start een thread voor elke client
    while( 1 )
    {
        printf( "\nWachten op verbinding...\n" );

        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof client_addr;
        socket_t client = accept( listen_sock,
                                  (struct sockaddr *)&client_addr,
                                  &client_len );
        if( client == INVALID_SOCKET_VALUE )
        {
            print_error( "accept" );
            continue;
        }

        client_counter++;
        printf( "Client %d verbonden!\n", client_counter );

        // Maak thread-gegevensstructuur aan en start thread voor deze client
        client_thread_args_t * args = (client_thread_args_t *)malloc( sizeof(client_thread_args_t) );
        if( args == NULL )
        {
            fprintf( stderr, "malloc failed\n" );
            CLOSE_SOCKET( client );
            continue;
        }
        args->client_sock = client;
        args->client_id = client_counter;

        
#ifdef _WIN32
        thread_t tid = _beginthreadex( NULL, 0, handle_client_thread, args, 0, NULL );
        if( tid == 0 )
        {
            perror( "_beginthreadex" );
            CLOSE_SOCKET( client );
            free( args );
            continue;
        }
        CloseHandle( (HANDLE)tid );
#else
        thread_t tid;
        if( pthread_create( &tid, NULL, handle_client_thread, args ) != 0 )
        {
            perror( "pthread_create" );
            CLOSE_SOCKET( client );
            free( args );
            continue;
        }
        pthread_detach( tid );
#endif
    }

    CLOSE_SOCKET( listen_sock );
    OSCleanup();
    return 0;
}
