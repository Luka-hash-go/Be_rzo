#include <mictcp.h>
#include <../include/api/mictcp_core.h>
#include <signal.h>

// Fenetre glissante
#define fenetreSize 10
uint8_t fenetre[fenetreSize];// 0 -> Echec, 1 -> Reussite. Par défaut tous les élements sont a 0. Cela permet de forcer les ré-envoie de paquet au début. Cependant,
// le BE étant destiné a être envoyé un grand nombre de paquet, cela va se stabiliser seulement après quelques émissions.

float tolerance = 0.0; // -> X% de réussite nécessaire
uint8_t indexTab = 0;

#define nbMaxSocket 10
unsigned short port = 5000;
mic_tcp_sock socketTab[nbMaxSocket];
int nbSocket = 0; // Permet de stocker le nombre de soscket déjà existant
int seq_num_recv = 0;
int seq_num_send = 0;
int numero_paquet = 0;


/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreurr
 */

void afficherTab() {
    printf("Tab : \n");
    for (int i = 0; i < fenetreSize; i++) {
        printf("%u, ", fenetre[i]);
    }
    printf("\n");
}

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
    set_loss_rate(5);

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
    
      mic_tcp_pdu syn_pdu, synack_pdu, ack_pdu;
      socketTab[socketID].remote_addr = *addr;

    // 1. Attendre SYN
    if (IP_recv(&syn_pdu, &socketTab[socketID].local_addr, &socketTab[socketID].remote_addr, 0) == -1 || syn_pdu.header.syn != 1) {
        printf("[MIC-TCP] Erreur : SYN non reçu\n");
        return -1;
    }

    // 2. Envoyer SYN-ACK
    memset(&synack_pdu, 0, sizeof(mic_tcp_pdu));
    synack_pdu.header.source_port = socketTab[socketID].local_addr.port;
    synack_pdu.header.dest_port = syn_pdu.header.source_port;
    synack_pdu.header.syn = 1;
    synack_pdu.header.ack = 1;
    IP_send(synack_pdu, remote);

    // 3. Attendre ACK
    if (IP_recv(&ack_pdu, &socketTab[socketID].local_addr, &socketTab[socketID].remote_addr, 2000) == -1 || ack_pdu.header.ack != 1) {
        printf("[MIC-TCP] Erreur : ACK non reçu\n");
        return -1;
    }
    

    printf("Receive connect\n");

    socketTab[socketID].state = CONNECTED;
    return 0; // Connexion acceptée
}
/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect (int socketID, mic_tcp_sock_addr addr) {
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");


    socketTab[socketID].remote_addr = addr;
    socketTab[socketID].local_addr;
    if (socketTab[socketID].remote_addr.ip_addr.addr_size == 0) {
        return -1; // :(
    }

    /*mic_tcp_pdu SYN;
    SYN.header.source_port= socketTab[socketID].local_addr.port;
    SYN.header.dest_port =socketTab[socketID].remote_addr.port;
    SYN.header.syn = 1;
    SYN.header.ack = 0;
    SYN.header.fin = 0;
    SYN.payload.size = 8;
    
       
    mic_tcp_pdu SYNACK;
    SYNACK.header.source_port= socketTab[socketID].local_addr.port;
    SYNACK.header.dest_port =socketTab[socketID].remote_addr.port;
    SYNACK.header.syn = 1;
    SYNACK.header.ack = 1;
    SYNACK.header.fin = 0;
    SYNACK.payload.size = 8;
    
    mic_tcp_pdu ACK;
    ACK.header.source_port= socketTab[socketID].local_addr.port;
    ACK.header.dest_port =socketTab[socketID].remote_addr.port;
    ACK.header.syn = 0;
    ACK.header.ack = 1;
    ACK.header.fin = 0;
    ACK.payload.size = 8;


   

    IP_send(SYN,socketTab[socketID].remote_addr.ip_addr);
    //Ensuite, on attend la réponse du serveur
    if(IP_recv(&SYNACK,&socketTab[socketID].local_addr,&socketTab[socketID].remote_addr,1000000));perror("erreur aucun synack envoyé");
    IP_send(ACK,socketTab[socketID].remote_addr.ip_addr);
    */
    mic_tcp_pdu syn_pdu, synack_pdu, ack_pdu;
    memset(&syn_pdu, 0, sizeof(mic_tcp_pdu));
    syn_pdu.header.source_port = socketTab[socketID].local_addr.port;
    syn_pdu.header.dest_port = addr.port;
    syn_pdu.header.syn = 1;

    // 1. Envoyer SYN
    IP_send(syn_pdu, addr.ip_addr);

    // 2. Attendre SYN-ACK
    if (IP_recv(&synack_pdu, &socketTab[socketID].local_addr.ip_addr, &socketTab[socketID].remote_addr.ip_addr, 2000) == -1
        || synack_pdu.header.syn != 1 || synack_pdu.header.ack != 1) {
        printf("[MIC-TCP] Erreur : SYN-ACK non reçu ou incorrect\n");
        return -1;
    }

    // 3. Envoyer ACK
    memset(&ack_pdu, 0, sizeof(mic_tcp_pdu));
    ack_pdu.header.source_port = socketTab[socketID].local_addr.port;
    ack_pdu.header.dest_port = addr.port;
    ack_pdu.header.ack = 1;
    IP_send(ack_pdu, addr.ip_addr);

    

    socketTab[socketID].state = CONNECTED; // Set state after validation
    tolerance = 0.8; // le client redefini le temps de tolérance
    return 0; // ca s'est bien passé ;)
}
   /*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size) {

    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_sock sock = socketTab[mic_sock];
    int sent_size;
    int nb_envoyes = 0;

    // Créer un mic_tcp_pdu
    mic_tcp_pdu pdu;

    // Correction : état CONNECTED (pas ESTABLISHED)
    if ((sock.fd == mic_sock) && (sock.state == CONNECTED)){
        pdu.header.source_port = sock.local_addr.port;
        pdu.header.dest_port = sock.remote_addr.port;
        pdu.header.seq_num = seq_num_send;
        pdu.header.ack = 0;
        pdu.header.syn = 0;
        pdu.header.fin = 0;
        pdu.payload.data = mesg;
        pdu.payload.size = mesg_size;

        int current_seq = seq_num_send;
        int ack_recu = 0;

        while (!ack_recu) {
            sent_size = IP_send(pdu, sock.remote_addr.ip_addr);
            printf("Envoi du paquet : %d, tentative n° : %d.\n", current_seq, nb_envoyes + 1);
            nb_envoyes++;
            mic_tcp_pdu ack_pdu;
            int timeout = 2000;

            // Wait for acknowledgment
            if (IP_recv(&ack_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, timeout) != -1) {
                // Correction : vérifier ack == 1 et ack_num == current_seq
                if (ack_pdu.header.ack == 1 && ack_pdu.header.ack_num == current_seq) {
                    printf("ACK reçu pour le paquet : %d\n", current_seq);
                    ack_recu = 1;
                    if (nb_envoyes == 1)fenetre[indexTab] = 1;
                } else {
                    printf("ACK incorrect reçu (ack_num: %d, attendu: %d)\n", ack_pdu.header.ack_num, current_seq);
                    if (nb_envoyes == 1) {
                        fenetre[indexTab] = 0;
                    }

                    // Deux echecs d'affilés
                    int index_precedent = (indexTab - 1 + fenetreSize ) % fenetreSize;
                    if (fenetre[index_precedent] == 0) {
                        printf(" Deux echecs d'affilés, on renvoie...");
                        continue;
                    }
                }
                afficherTab();
            } else {
                uint8_t somme = 0;
                for (int i = 0; i < fenetreSize; i++) {
                    somme += fenetre[i];
                }
                if (somme/fenetreSize >= tolerance) {
                    ack_recu = 1;
                    printf("On est sous la tolerence. Pas grave le paquet est négligé.");
                    fenetre[indexTab] = 0;
                    afficherTab();
                    indexTab = (indexTab + 1) % fenetreSize;
                    return 0;
                }
                printf("Timeout - Réémission nécessaire pour le paquet : %d\n", current_seq);
                fenetre[indexTab] = 0;
            }
        }
        seq_num_send = (seq_num_send + 1) % 2;
        indexTab = (indexTab + 1) % fenetreSize; //index fenetre 
        return sent_size;
    }
    return -1;
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
        fprintf(stderr, "[MIC-TCP] Erreur : socketID %d invalide dans close\n", socketID);
        return -1;
    }

    socketTab[socketID].state = IDLE;
    // Correction : ne pas appeler close() sur fd, juste marquer comme fermé
    socketTab[socketID].fd = -1;

    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put()..
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr) {
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    // Correction : traiter le PDU reçu selon le numéro de séquence attendu

    if(pdu.header.syn == 1 && pdu.header.ack == 0){

        mic_tcp_pdu SYNACK;
        SYNACK.header.source_port= pdu.header.source_port;
        SYNACK.header.dest_port =pdu.header.dest_port;
        SYNACK.header.syn = 1;
        SYNACK.header.ack = 1;
        SYNACK.payload.size = 8;
        IP_send(SYNACK,remote_addr);
    }
    
    if (pdu.header.seq_num == seq_num_recv) {
        // Insertion des données utiles dans le buffer de réception
        app_buffer_put(pdu.payload);

        // Construire un PDU d'ACK pour ce numéro de séquence
        mic_tcp_pdu ack_pdu;
        memset(&ack_pdu, 0, sizeof(mic_tcp_pdu));
        ack_pdu.header.source_port = pdu.header.dest_port;
        ack_pdu.header.dest_port   = pdu.header.source_port;
        ack_pdu.header.ack         = 1;
        ack_pdu.header.ack_num     = seq_num_recv;

        IP_send(ack_pdu, remote_addr);

        seq_num_recv = (seq_num_recv + 1) % 2;
    } else {
        printf("PDU reçu hors séquence (reçu: %d, attendu: %d) — ACK du précédent renvoyé\n", pdu.header.seq_num, seq_num_recv);

        // Réenvoyer un ACK pour le dernier paquet reçu correctement
        mic_tcp_pdu ack_pdu;
        memset(&ack_pdu, 0, sizeof(mic_tcp_pdu));
        ack_pdu.header.source_port = pdu.header.dest_port;
        ack_pdu.header.dest_port   = pdu.header.source_port;
        ack_pdu.header.ack         = 1;
        ack_pdu.header.ack_num     = (seq_num_recv + 1) % 2; // dernier reçu correctement

        IP_send(ack_pdu, remote_addr);
    }
}


