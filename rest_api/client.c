#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"

#define HOST_IP "3.8.116.10" //IP-server in internet
#define raspuns(str) printf("Operatia %s efectuata cu succes\n", str);

// Returneaza cookie de sesiune
char *get_cookies(char *str) {
	char *p = strstr(str, "Date:");
	*p = '\0';
	return str;
}

// Extrage din mesajul de la server tokenul jwt
char *get_token_jwt(char *str) {
	char *p = strstr(str, "{\"token\":");
	p = p + 10;
	char *d = strstr(p, "\"");
	*d = '\0';
	return p;
}

// Functia citeste o carte de la tastatura
char *read_book() {
	char *carte = malloc(sizeof(char) * 1024);
	char title[64], author[64], genre[64], publisher[64];
	int page_count;
	scanf("%s %s %s %d %s", title, author, genre, &page_count, publisher);
	sprintf(carte, "{\n\t\"title\": \"%s\",\n\t\"author\": \"%s\",\n\t\"genre\": \"%s\",\n\t\"page_count\": \"%d\",\n\t\"publisher\": \"%s\"\n}\r\r\n",
				title, author, genre, page_count, publisher);
	return carte;
}

// Functia cauta un string in alt string
// daca rezultatul nu e null insemamna
// ca una din operatii a fost efectuata cu succes
char *check_error(char *message, char *str) {
	return strstr(message, str);
}

int main(int argc, char *argv[]) {
    char *message, *response, comand[32];
    int sockfd;
	char *line = calloc(LINELEN, sizeof(char));
    char *cookies = NULL;
    char *token_jwt = NULL;
	char login[16], passw[16];
	int book_id;
	char *carte = malloc(sizeof(char) * 1024);
	int logat = -1;
	int access_biblioteca = -1;
	// In while citesc comenzile de la stdin si transmit la server
	while(1) {
		memset(comand, 0, 32);
		fscanf(stdin, "%s", comand);
		sockfd = open_connection(HOST_IP, 8080, AF_INET, SOCK_STREAM, 0);
		if(memcmp(comand, "register", 8) == 0) {
			if(logat == 1) {
				printf("Nu te poti registra, fa initial logout\n");
				continue;
			}
			printf("username=");
			scanf("%s", login);
			printf("password=");
			scanf("%s", passw);
			sprintf(line,  "{\"username\":\"%s\",\"password\":\"%s\"}", login, passw);
			message = compute_post_request(HOST_IP, "/api/v1/tema/auth/register","application/json", &line, 1, NULL, 0);
			send_to_server(sockfd, message);
			response = receive_from_server(sockfd);
			// Verific daca am primit raspuns bun de la server
			if(check_error(response, "HTTP/1.1 201 Created") == NULL) {
				printf("Userul cu username-ul: [%s] este deja registrat\n", login);
				continue;
			}
			raspuns("register");
			close_connection(sockfd);
		} else if(memcmp(comand, "login", 5) == 0) {
			if(logat == 1) {
				printf("Esti deja logat!\n");
				continue;
			}
			printf("username=");
			scanf("%s", login);
			printf("password=");
			scanf("%s", passw);

			sprintf(line,  "{\"username\":\"%s\",\"password\":\"%s\"}", login, passw);
			message = compute_post_request(HOST_IP, "/api/v1/tema/auth/login","application/json", &line, 1, NULL, 0);
			send_to_server(sockfd, message);
			response = receive_from_server(sockfd);
			close_connection(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				raspuns("login");
				// Extrag cookie de sesiune
				cookies = get_cookies(strstr(response, "Set-Cookie: ") + 11);
				logat = 1;
			} else {
				printf("Eroare la login, username sau parola gresita\n");
			}
		} else if(memcmp(comand, "enter_library", 13) == 0) {
			if(logat == -1) {
				printf("Pentru a intra in biblioteca trebuie sa fii logat\n");
				continue;
			}
			message = compute_get_request(HOST_IP, "/api/v1/tema/library/access", "application/json", &cookies, 1);
			send_to_server(sockfd, message);
			response = receive_from_server(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				token_jwt = get_token_jwt(response);
				raspuns("enter_library");
				access_biblioteca = 1;
			} else {
				printf("Eroare nu ai acces la biblioteca\n");
			}
			close_connection(sockfd);
		} else if(memcmp(comand, "get_books", 9) == 0) {
			if(access_biblioteca == -1) {
				printf("Nu aveti acces la biblioteca\n");
				continue;
			}
			char *vector = malloc(sizeof(char) * 1024);
			char *help = malloc(sizeof(char) * 1024);
			strcat(vector, "GET /api/v1/tema/library/books HTTP/1.1\r\n");
			strcat(vector, "Host: 3.8.116.10\r\n");
			sprintf(help, "Authorization: Bearer %s\r\n\r\n", token_jwt);
			strcat(vector, help);
			send_to_server(sockfd, vector);
			response = receive_from_server(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				raspuns("get_books");
				printf("%s\n", response);
			} else {
				printf("Eroare nu ai acces la biblioteca\n");
			}
			close_connection(sockfd);
		} else if(memcmp(comand, "get_book", 8) == 0) {
			if(access_biblioteca == -1) {
				printf("Nu aveti acces la biblioteca\n");
				continue;
			}
			printf("book_id=");
			scanf("%d", &book_id);
			char *v = malloc(sizeof(char) * 1024);
			char *h = malloc(sizeof(char) * 1024);
			sprintf(v, "GET /api/v1/tema/library/books/%d HTTP/1.1\r\n", book_id);
			strcat(v, "Host: 3.8.116.10\r\n");
			sprintf(h, "Authorization: Bearer %s\r\n\r\n", token_jwt);
			strcat(v, h);
			send_to_server(sockfd, v);
			response = receive_from_server(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				raspuns("get_book");
				printf("%s\n", response);
			} else {
				printf("Nu exista asa carte\n");
			}
			close_connection(sockfd);
		} else if(memcmp(comand, "add_book", 8) == 0) {
			if(access_biblioteca == -1) {
				printf("Nu aveti acces la biblioteca\n");
				continue;
			}
			char *h = malloc(sizeof(char) * 1024);
			char *v = malloc(sizeof(char) * 1024);
			sprintf(h, "Authorization: Bearer %s", token_jwt);
			strcat(v, HOST_IP);
			strcat(v, "\r\n");
			strcat(v, h);
			carte = read_book();
			message = compute_post_request(v, "/api/v1/tema/library/books","application/json", &carte, 1, NULL, 0);
			send_to_server(sockfd, message);
			response = receive_from_server(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				printf("Carte adaugata cu succes!\n");
			} else {
				if(check_error(response, "HTTP/1.1 429 Too Many Requests") != NULL) {
					printf("In ultimul timp ai adaugat pre multe carti\n");
				} else {
					printf("Datele sunt incomplete!\n");
				}
			}
			close_connection(sockfd);
		} else if(memcmp(comand, "delete_book", 11) == 0) {
			if(access_biblioteca == -1) {
				printf("Nu aveti acces la biblioteca\n");
				continue;
			}
			printf("book_id pentru stergere=");
			scanf("%d", &book_id);
			char *v = malloc(sizeof(char) * 1024);
			char *h = malloc(sizeof(char) * 1024);
			sprintf(v, "DELETE /api/v1/tema/library/books/%d HTTP/1.1\r\n", book_id);
			strcat(v, "Host: 3.8.116.10\r\n");
			sprintf(h, "Authorization: Bearer %s\r\n\r\n", token_jwt);
			strcat(v, h);
			send_to_server(sockfd, v);
			response = receive_from_server(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				printf("Cartea este stearsa cu succes!\n");
			} else {
				printf("Cartea nu exista!\n");
			}
			close_connection(sockfd);
		} else if(memcmp(comand, "logout", 6) == 0) {
			if(logat == -1) {
				printf("Nu esti logat\n");
				continue;
			}
			message = compute_get_request(HOST_IP, "/api/v1/tema/auth/logout", "application/json", &cookies, 1);
			send_to_server(sockfd, message);
			response = receive_from_server(sockfd);
			if(check_error(response, "HTTP/1.1 200 OK") != NULL) {
				raspuns("logout	");
				logat = -1;
				access_biblioteca = -1;
			}
			close_connection(sockfd);
		} else if(memcmp(comand, "exit", 4) == 0) {
			printf("La revedere!\n");
			return 0;
		} else {
			// Comanda necunoscuta
			printf("Comanda invalida\n");
		}
	}
}