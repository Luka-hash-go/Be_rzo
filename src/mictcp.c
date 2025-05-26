#include <mictcp.h>
#include <api/mictcp_core.h>
#include <pthread.h>

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

pthread_mutex_t mutex_reception = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_reception = PTHREAD_COND_INITIALIZER;
char* buffer_reception;
short nouvelle_donnee = 0;
int seq_number = 0;

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreurr
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

    set_loss_rate(0.5);

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

    /*mic_tcp_pdu pduAccept;

    pduAccept.header.source_port = port;
    pduAccept.header.dest_port = port;

    printf("<- Syn\n");
    printf("Local : %s, Dest : %s \n", socketTab[socketID].local_addr.ip_addr.addr, NULL);
    int result = IP_recv(&pduAccept, &socketTab[socketID].local_addr.ip_addr, &addr->ip_addr, 1000);

    if (result < 0 || pduAccept.header.syn == '0') {
        fprintf(stderr, "Syn non recu");
    }


    pduAccept.header.syn = '1';
    pduAccept.header.ack = '1';
    printf("-> SynAck");
    IP_send(pduAccept, addr->ip_addr);
    printf("<- Ack");
    result = IP_recv(&pduAccept, &socketTab[socketID].local_addr.ip_addr, &addr->ip_addr, 1000);

    if (result < 0 || pduAccept.header.ack == '0') {
        fprintf(stderr, "Ack non recu");
    }*/

    printf("Receive connect\n");
    socketTab[socketID].state = CONNECTED;

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
    socketTab[socketID].state = CONNECTED;


    if (socketTab[socketID].remote_addr.ip_addr.addr_size == 0) {
        return -1; // :(
    }

    return 0; // ca s'est bien passé ;)
}
   /*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size) {
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_sock sock = socketTab[mic_sock];

    // Vérifie que l'adresse distante a été définie
    if (sock.remote_addr.ip_addr.addr_size <= 0) {
        fprintf(stderr, "[MIC-TCP] Erreur : adresse distante non définie (mic_tcp_connect oublié ?)\n");
        return -1;
    }
    // Créer un mic_tcp_pdu
    mic_tcp_pdu pdu; // grace à la structure pdu_mic_tcp
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;
    pdu.header.seq_num = 0;

    pdu.header.source_port = sock.local_addr.port;  // logique : envoyer depuis le port bindé oblige de apsser aevc des pointeur car si on modifie une structure global ce qu'on faisait avant
    // etait juste une copie local  dans la pile de la fonction detruite a la fin de celle ci 
    pdu.header.dest_port = sock.remote_addr.port;

    int timeout = 5000;
    int ack_recu = 0;
    int sent_size = -1;
    mic_tcp_pdu ack;
    ack.payload.size = 8;

    mic_tcp_ip_addr src;
    mic_tcp_ip_addr dst;

    mic_tcp_sock_addr ack_addr;
    static int seq_num = 0; // Numéro de séquence pour Stop-and-Wait
    pdu.header.ack = '0'; // on initialise le flag ack  

    pdu.header.seq_num = seq_num;

    while (!ack_recu) {
        sent_size = IP_send(pdu, sock.remote_addr.ip_addr); // <--

        if (IP_recv(&ack, &src, &dst, timeout) != -1 && ack.header.ack == '1'&& ack.header.seq_num == seq_num) {
            ack_recu = 1;
        }
    }
     seq_num = (seq_num + 1) % 2; // Alterne entre 0 et 1

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

    pthread_mutex_lock(&mutex_reception);
    while (nouvelle_donnee == 0) pthread_cond_wait(&cond_reception, &mutex_reception);

    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;
    int effective_data_size = app_buffer_get(payload);

    nouvelle_donnee = 0;
    pthread_mutex_unlock(&mutex_reception);

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
    static int expected_seq = 0; // Numéro de séquence attendu pour Stop-and-Wait
    if (pdu.header.dest_port == port && pdu.header.seq_num == seq_number && pdu.header.seq_num == expected_seq) {
        pthread_mutex_lock(&mutex_reception);
        app_buffer_put(pdu.payload); // on traite le pdu c'est cool :o
        nouvelle_donnee = 1;
        pthread_cond_signal(&cond_reception);
        pthread_mutex_unlock(&mutex_reception);
    
        mic_tcp_pdu ack;
        ack.payload.size = 8;
        ack.header.ack = '1';
        ack.header.seq_num = expected_seq; // On acquitte le bon numéro de séquence
        IP_send(ack, remote_addr);
        expected_seq = (expected_seq + 1) % 2; // Alterne entre 0 et 1
    }
    else {
        printf("Paquet incorrect recu ! \n");
    }


}
