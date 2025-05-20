#include <mictcp.h>
#include <api/mictcp_core.h>


// typedef struct mic_tcp_sock
// {
//   int fd;  /* descripteur du socket */
//   protocol_state state; /* état du protocole */
//   mic_tcp_sock_addr local_addr; /* adresse locale du socket */
//   mic_tcp_sock_addr remote_addr; /* adresse distante du socket */
// } mic_tcp_sock;

#define nbMaxSocket 10
unsigned short port = 5000;
mic_tcp_sock socketTab[nbMaxSocket];
int nbSocket = 0; // Permet de stocker le nombre de soscket déjà existant
/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

int mic_tcp_socket(start_mode sm) {
    mic_tcp_sock mon_socket;

    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    int result = initialize_components(sm); /* Appel obligatoire et initialise les compo de l'api */ 
    

    if (result < 0) {
        return -1; // Échec de l'initialisation
    }

    if (nbSocket >= nbMaxSocket) {
        fprintf(stderr, "[MIC-TCP] Erreur : nombre maximum de sockets atteint\n");
        return -1;
    }

    // Initialiser le socket
    mon_socket.fd = nbSocket;
    mon_socket.state = IDLE;

    // Ajout du socket dans le tableau associé
    socketTab[mon_socket.fd] = mon_socket;
    nbSocket++;

    return mon_socket.fd; // Retourner l’identifiant du socket
}

/*
 * Nous avons ajouté une fonction bind, afin d'associer la representation interne du socket avec son adresse
*/
int mic_tcp_bind(int socketID, mic_tcp_sock_addr addr) { // oblige en reception
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    // On commence par récupérer le socket souhaitait dans le tableau
    socketTab[socketID].local_addr = addr;
    socketTab[socketID].state = IDLE;

    return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socketID, mic_tcp_sock_addr* addr) {
    printf("[MIC-TCP] Appel de la fonction : %s\n", __FUNCTION__);

    // mic_tcp_sock socket = socketTab[socketID];

    // // on a pas reellement de phase de connection
    // socket.state = CONNECTED;

    // // On copie l'adresse 
    // if (addr != NULL) {
    //     socket.remote_addr.ip_addr = addr->ip_addr;
    //     socket.remote_addr.port = addr->port;
    // }

    return 0; // Connexion acceptée
}
/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect (int socketID, mic_tcp_sock_addr addr) {
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    // Stocker l’adresse et le port de destination passés par addr dans la structure mictcp_socket correspondant au socket identifié par socket passé en paramètre.
    socketTab[socketID].remote_addr = addr;
    socketTab[socketID].state = ESTABLISHED;

    if (socketTab[socketID].remote_addr.ip_addr.addr_size == 0) {
        return -1; // :(
    }

    //memcpy(addr.ip_addr.addr,socket.remote_addr.ip_addr.addr, socket.remote_addr.ip_addr.addr_size);

    // socket.state = CONNECTED;

    return 0; // ca s'est bien passé ;)
}
   /*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size) {
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_sock *sock = &socketTab[mic_sock];

    // Vérifie que l'adresse distante a été définie
    if (sock->remote_addr.ip_addr.addr_size <= 0) {
        fprintf(stderr, "[MIC-TCP] Erreur : adresse distante non définie (mic_tcp_connect oublié ?)\n");
        return -1;
    }
    // Créer un mic_tcp_pdu
    mic_tcp_pdu pdu; // grace à la structure pdu_mic_tcp
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    pdu.header.source_port = sock->local_addr.port;  // logique : envoyer depuis le port bindé oblige de apsser aevc des pointeur car si on modifie une structure global ce qu'on faisait avant
    // etait juste une copie local  dans la pile de la fonction detruite a la fin de celle ci 
    pdu.header.dest_port = sock->remote_addr.port;

//Envoyer un message (dont la taille le contenu sont passés en paramètres).
    int sent_size = IP_send(pdu,sock->remote_addr.ip_addr); //structure mictcp_socket_addr contenue dans la structure mictcp_socket correspondant au socket identifié par mic_sock passé en paramètre).
    return sent_size;
}



 /*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv(int socket, char* mesg, int max_mesg_size) {
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    if (mesg == NULL || max_mesg_size <= 0) {
        fprintf(stderr, "[MIC-TCP] Erreur : buffer invalide dans recv\n");
        return -1;
    }

    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;

    int effective_data_size = app_buffer_get(payload);
    return effective_data_size;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socketID) {
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
   
    if (socketID < 0 || socketID >= nbSocket) {
        fprintf(stderr, "[MIC-TCP] Erreur : socketID %d invalide dans close\n", socketID); // seule erreur possible que je vois lors d'un close :(
        return -1;
    }

    // on passe ne idle on finie
    socketTab[socketID].state = IDLE;
    close(socketTab[socketID].fd);
    socketTab[socketID].fd = -1; // on le marque comme fermé on le reutilisera mais on mettra de nouveau à jour son fd etc ...

    return 0; // on simule la fermeture 
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr) {
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    // Si le port de destination correspond à notre socket
    for (int i = 0; i < nbSocket; i++) {
        // On travaille directement sur la copie locale de socketTab[i] pas necessaire de travailler avec le tab directement on veut juste lire pas ecrire ;)
        mic_tcp_sock socket = socketTab[i];

        if (socket.state != IDLE) {
            if (pdu.header.dest_port == socket.local_addr.port) {
                app_buffer_put(pdu.payload); // on traite le pdu c'est cool :o
                
            }
        }
    }

    fprintf(stderr, "[MIC-TCP] PDU reçu pour port inconnu: %d\n", pdu.header.dest_port);
}
