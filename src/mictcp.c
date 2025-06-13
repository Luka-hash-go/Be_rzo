#include <mictcp.h>
#include <../include/api/mictcp_core.h>
#include <pthread.h>



/*


*/

// Fenetre glissante
#define fenetreSize 10
uint8_t fenetre[fenetreSize];// 0 -> Echec, 1 -> Reussite. Par défaut tous les élements sont a 0. Cela permet de forcer les ré-envoie de paquet au début. Cependant,
// le BE étant destiné a être envoyé un grand nombre de paquet, cela va se stabiliser seulement après quelques émissions...
uint8_t indexTab=0;
float tolerance = 0.8; // -> X% de réussite nécessaire (sera négociéplus tard ;))
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER; // Pour la 4.2
pthread_cond_t  conn_cond   = PTHREAD_COND_INITIALIZER;   // Pour la 4.2 accept/connexion portection contre l'asynchronisme

#define nbMaxSocket 10
mic_tcp_sock socketTab[nbMaxSocket];
int nbSocket = 0; // Permet de stocker le nombre de soscket déjà existant
int seq_num_recv = 0;
int seq_num_send = 0;
int numero_paquet = 0;

// Ajout d'une variable d'état pour la connexion côté serveur
int server_connection_established = 0;
mic_tcp_sock_addr last_client_addr;


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

    // Initialisation du socket
    mon_socket.fd = nbSocket;
    mon_socket.state = IDLE;

    // Ajout du socket dans le tableau associé
    socketTab[mon_socket.fd] = mon_socket;
    nbSocket++;
    set_loss_rate(5);

    return mon_socket.fd; // Retourne l’identifiant du socket !
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

    pthread_mutex_lock(&buffer_mutex);
    server_connection_established = 0; //  test patch 
    while (!server_connection_established) {
        pthread_cond_wait(&conn_cond, &buffer_mutex);
    }
    // On recupere  l'adresse du client qui vient de se connecter
    *addr = last_client_addr;
    socketTab[socketID].remote_addr = *addr;
    socketTab[socketID].state = CONNECTED;
    server_connection_established = 0; // Reset pour prochaine connexion
    pthread_mutex_unlock(&buffer_mutex);

    printf("Receive connect\n");
    return 0; // Connexion acceptée
}
/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect (int socketID, mic_tcp_sock_addr addr) {
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    // Phase d'établissement de connexion (client)
    mic_tcp_pdu syn_pdu, synack_pdu, ack_pdu;
    int timeout = 3000;
    int success = 0; // patch pour erreur syn aleatoire 
    int max_retries = 5; //arbitrairement avec le loss_rate
    int retry = 0;  

    // Envoi SYN avec tolérance demandée dans le payload
    memset(&syn_pdu, 0, sizeof(mic_tcp_pdu));
    syn_pdu.header.syn = 1;
    syn_pdu.header.seq_num = 42; // arbitraire on aurait pu mettre ce que l'on veut cela permet d'eviter les paquets fantomes
    syn_pdu.payload.data = (char*)&tolerance; //on envoie dans le syn la tolerance
    syn_pdu.payload.size = sizeof(float);
    while (retry < max_retries && !success) { // patch erreur connection
        IP_send(syn_pdu, addr.ip_addr);

        // Attente SYN-ACK
        if (IP_recv(&synack_pdu, &socketTab[socketID].local_addr.ip_addr, &addr.ip_addr, timeout) == -1 || synack_pdu.header.syn != 1 || synack_pdu.header.ack != 1) {
            printf("Erreur : SYN-ACK non reçu\n");
            return -1;
        }else {
        printf("Tentative %d : SYN-ACK non reçu, on recommence...\n", retry+1);
        retry++;
        }
        
    
    }
    //patch 
    if (!success) {
    printf("Erreur : SYN-ACK non reçu après %d tentatives\n", max_retries);
    return -1;
    }

    // Envoi ACK final
    memset(&ack_pdu, 0, sizeof(mic_tcp_pdu));
    ack_pdu.header.ack = 1;
    ack_pdu.header.ack_num = synack_pdu.header.seq_num;
    IP_send(ack_pdu, addr.ip_addr);

    socketTab[socketID].remote_addr = addr;
    if (socketTab[socketID].remote_addr.ip_addr.addr_size == 0) {
        return -1; // :(
    }

    socketTab[socketID].state = CONNECTED; // On met le socket à connecté 

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

            // attente d'acquitement
            if (IP_recv(&ack_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, timeout) != -1) {
                // Correction effectue et probleme : vérifier ack == 1 et ack_num == current_seq
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
    // Correction effectué ne pas appeler close() sur fd, juste marquer comme fermé
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

    pthread_mutex_lock(&buffer_mutex); // 4.2 : protection du buffer

    // Gestion de l'établissement de connexion côté serveur
    if (pdu.header.syn == 1 && pdu.header.ack == 0) {
        // SYN reçu, négociation de tolérance imposé par le sender
        if (pdu.payload.size == sizeof(float)) {
            memcpy(&tolerance, pdu.payload.data, sizeof(float));
            printf("Tolérance négociée (serveur) : %f\n", tolerance);
        }
        // Envoyer SYN-ACK
        mic_tcp_pdu synack_pdu;
        memset(&synack_pdu, 0, sizeof(mic_tcp_pdu));
        synack_pdu.header.syn = 1;
        synack_pdu.header.ack = 1;
        synack_pdu.header.ack_num = pdu.header.seq_num;
        IP_send(synack_pdu, remote_addr);

        // Attendre ACK final (simplifié : on suppose qu'il arrive ensuite)
        // On ne bloque pas ici, on attendra le ACK dans un prochain appel à process_received_PDU
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }
    if (pdu.header.syn == 1 && pdu.header.ack == 1) {
        // SYN-ACK reçu côté client, rien à faire ici côté serveur
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }
    if (pdu.header.ack == 1 && pdu.header.syn == 0) {
        // ACK final reçu côté serveur, signaler à accept que la connexion est établie
        server_connection_established = 1;
        // Remplir l'adresse du client pour accept
        last_client_addr.ip_addr = remote_addr;
        // On suppose que le port source du client est dans pdu.header.source_port
        last_client_addr.port = pdu.header.source_port;
        // pthread_cond_signal(&conn_cond); patch test 
        pthread_cond_broadcast(&conn_cond); // patch test
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }

    // ...4.2 : Trouver le bon socket en fonction du port et de l'adresse...
    int found = 0;
    int sock_idx = -1;
    for (int i = 0; i < nbSocket; i++) {
        if (socketTab[i].local_addr.port == pdu.header.dest_port &&
            socketTab[i].remote_addr.ip_addr.addr == remote_addr.addr) {
            sock_idx = i;
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("Aucun socket correspondant trouvé pour ce PDU\n");
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }

    // Correction : traiter le PDU reçu selon le numéro de séquence attendu
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

    pthread_mutex_unlock(&buffer_mutex); // 4.2 : protection du buffer
}


