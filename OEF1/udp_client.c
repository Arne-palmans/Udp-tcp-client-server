#ifdef _WIN32
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <ctype.h>
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
    #include <ctype.h>
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
//activeerd cleanup van de Windows Sockets API, waarbij alle resources worden vrijgegeven die door de API zijn toegewezen.
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
// Maak UDP socket en vul srv_addr/srv_len met adres van server
static socket_t make_socket( struct sockaddr ** srv_addr, socklen_t * srv_len )
{
    struct addrinfo hints, *res;
    memset( &hints, 0, sizeof hints );
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if( getaddrinfo( "127.0.0.1", "5500", &hints, &res ) != 0 )
    { fprintf( stderr, "getaddrinfo mislukt\n" ); exit( 1 ); }
    
    socket_t s = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
    if( s == INVALID_SOCKET_VALUE ) { print_error( "socket" ); exit( 1 ); }

    *srv_len  = res->ai_addrlen;
    *srv_addr = malloc( res->ai_addrlen );
    memcpy( *srv_addr, res->ai_addr, res->ai_addrlen );
    freeaddrinfo( res );
    return s;
}

static void send_msg( socket_t sock, const char * msg,
                      const struct sockaddr * addr, socklen_t len )
{
    if( sendto( sock, msg, (int)strlen( msg ), 0, addr, len ) == SOCKET_ERROR_VALUE )
        print_error( "sendto" );
}

// Probeer een regel tekst van de gebruiker te lezen zonder te blokkeren.
static bool try_read_line( char * out, int outsz )
{
#ifdef _WIN32
    HANDLE h = GetStdHandle( STD_INPUT_HANDLE );
    DWORD events = 0;
    if( !GetNumberOfConsoleInputEvents( h, &events ) || events == 0 )
        return false;
    if( fgets( out, outsz, stdin ) == NULL ) return false;
    out[ strcspn( out, "\r\n" ) ] = '\0';
    return out[0] != '\0';
#else
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( 0, &fds );
    struct timeval tv = { 0, 0 };
    if( select( 1, &fds, NULL, NULL, &tv ) <= 0 ) return false;
    if( fgets( out, outsz, stdin ) == NULL ) return false;
    out[ strcspn( out, "\r\n" ) ] = '\0';
    return out[0] != '\0';
#endif
}
// Controleer of de string een geldige integer is (optioneel voorafgegaan door '-')
static bool is_integer( const char * s )
{
    if( *s == '-' ) s++;
    if( *s == '\0' ) return false;
    while( *s ) { if( *s < '0' || *s > '9' ) return false; s++; }
    return true;
}

void execution( socket_t sock, struct sockaddr * srv, socklen_t srvlen )
{
    char buf[256];
    char line[64];

    while( true )   /* herhaal voor elke ronde */
    {
        printf( "\n========================================\n" );
        printf( "Voer een getal in en druk Enter: " );
        fflush( stdout );

        
        bool round_done = false;

        while( !round_done )
        {

            // Check of de gebruiker iets getypt heeft 
            if( try_read_line( line, sizeof line ) )
            {
                if( is_integer( line ) )
                {
                    send_msg( sock, line, srv, srvlen );
                    printf( "Gok verstuurd: %s\n", line );
                    printf( "Voer nog een getal in (of wacht op resultaat): " );
                    fflush( stdout );
                }
                else
                {
                    printf( "Ongeldige invoer — geef een integer.\n" );
                    printf( "Voer een getal in: " );
                    fflush( stdout );
                }
            }

            // Check of de server iets gestuurd heeft (100 ms wachten)
            fd_set fds;
            FD_ZERO( &fds );
            FD_SET( sock, &fds );
            struct timeval tv = { 0, 100000 };  /* 100 ms */
            int r = select( (int)sock + 1, &fds, NULL, NULL, &tv );

            if( r > 0 && FD_ISSET( sock, &fds ) )
            {
                struct sockaddr_storage src;
                socklen_t srclen = sizeof src;
                int n = recvfrom( sock, buf, sizeof buf - 1, 0,
                                  (struct sockaddr *)&src, &srclen );
                if( n > 0 )
                {
                    buf[n] = '\0';
                    printf( "\nServer: %s\n", buf );
                    fflush( stdout );

                   
                    if( strstr( buf, "You won !" ) != NULL ||
                        strstr( buf, "You lost !" ) != NULL )
                    {
                        round_done = true;
                    }
                    else if( strstr( buf, "You won ?" ) != NULL )
                    {
                       
                        printf( "(Stuur niets meer om te winnen, of gok verder om te verliezen)\n" );
                        fflush( stdout );
                    }
                }
            }
        }

        /* Ronde klaar — vraag of de gebruiker nog een ronde wil */
        printf( "\nNog een ronde spelen? (j/n): " );
        fflush( stdout );

        /* Wacht blokkering op antwoord */
        if( fgets( line, sizeof line, stdin ) == NULL ) break;
        line[ strcspn( line, "\r\n" ) ] = '\0';
        if( tolower( (unsigned char)line[0] ) != 'j' )
        {
            printf( "Tot ziens!\n" );
            break;
        }
    }
}

void cleanup( socket_t sock, struct sockaddr * srv )
{
    // Vrijgemaakt van geheugen en sluiten van socket
    free( srv );
    CLOSE_SOCKET( sock );
}

int main( void )
{
    OSInit();
    struct sockaddr * srv = NULL;
    socklen_t srvlen = 0;
    socket_t sock = make_socket( &srv, &srvlen );
    execution( sock, srv, srvlen );
    cleanup( sock, srv );
    OSCleanup();
    return 0;
}
