#include <mictcp.h>
#include <../include/api/mictcp_core.h>

#define MAX_SOCKETS 10
#define PERTES_ADM 0

// Tableau de sockets
mic_tcp_sock sockets[MAX_SOCKETS];

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 */
int mic_tcp_socket(start_mode sm)
{
    int result = -1;
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if ((result = initialize_components(sm)) == -1) return -1;

    set_loss_rate(PERTES_ADM);

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].state == CLOSED || sockets[i].state == IDLE) {
            sockets[i].fd = i;
            sockets[i].state = IDLE;
            return i;
        }
    }

    return -1; // Aucun socket libre
}

/*
 * Permet d’attribuer une adresse à un socket.
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if (socket >= 0 && socket < MAX_SOCKETS) {
        memcpy(&sockets[socket].addr, &addr, sizeof(mic_tcp_sock_addr));
        return 0;
    }

    return -1;
}

/*
 * Met le socket en état d'acceptation de connexions
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if (socket >= 0 && socket < MAX_SOCKETS && sockets[socket].state != CLOSED) {
        sockets[socket].state = ESTABLISHED;
        if (addr) memcpy(addr, &sockets[socket].addr, sizeof(mic_tcp_sock_addr));
        return 0;
    }

    return -1;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if (socket >= 0 && socket < MAX_SOCKETS && sockets[socket].state != CLOSED) {
        sockets[socket].state = ESTABLISHED;
        sockets[socket].addr = addr; // destination
        return 0;
    }

    return -1;
}

/*
 * Envoie une donnée
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if (mic_sock < 0 || mic_sock >= MAX_SOCKETS || sockets[mic_sock].state != ESTABLISHED)
        return -1;

    mic_tcp_pdu PDU;
    PDU.header.source_port = sockets[mic_sock].addr.port;
    PDU.header.dest_port   = sockets[mic_sock].addr.port; // peut être changé selon usage
    PDU.header.syn = 0;
    PDU.header.ack = 0;
    PDU.header.fin = 0;

    PDU.payload.data = mesg;
    PDU.payload.size = mesg_size;

    return IP_send(PDU, sockets[mic_sock].addr);
}

/*
 * Récupère une donnée depuis le buffer
 */
int mic_tcp_recv(int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if (socket < 0 || socket >= MAX_SOCKETS || sockets[socket].state != ESTABLISHED)
        return -1;

    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;

    sockets[socket].state = IDLE;
    int bytes_read = app_buffer_get(payload);
    sockets[socket].state = ESTABLISHED;

    return bytes_read;
}

/*
 * Ferme un socket
 */
int mic_tcp_close(int socket)
{
    printf("[MIC-TCP] Appel de la fonction : %s\n", __FUNCTION__);

    if (socket >= 0 && socket < MAX_SOCKETS && sockets[socket].state == ESTABLISHED) {
        sockets[socket].state = CLOSED;
        return 0;
    }

    return -1;
}

/*
 * Traitement d’un PDU MIC-TCP reçu
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    app_buffer_put(pdu.payload);

    // Associer dynamiquement à un socket (simplification ici)
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].state == IDLE || sockets[i].state == ESTABLISHED) {
            sockets[i].state = ESTABLISHED;
            break;
        }
    }
}
